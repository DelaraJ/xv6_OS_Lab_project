<div dir="rtl" style="text-align: right;">

# پیاده سازی Copy-on-Write (COW) در xv6 

## زیرساخت Reference Counting در kalloc.c
 یک آرایه‌ی سراسری int page_ref[PHYSTOP / PGSIZE] (مشابه، با یک spinlock مخصوص خودش به نام reflock) اضافه کردم که برای هر صفحه فیزیکی شمارنده نگه دارد. سه تابع جدید نوشتم: incref(pa) که شمارنده را یک واحد زیاد میکند، decref(pa) که یک واحد کم میکند و اگر به صفر رسید واقعا kfree را صدا میزند، و یک ماکرو PA2IDX(pa) برای تبدیل آدرس فیزیکی به ایندکس آرایه. در kalloc() وقتی صفحه‌ ی جدید تخصیص می یابد، رفرنس‌ کانتش را ۱ قرار میدهم. در kfree() این تابع دیگر مستقیم صدا زده نمیشود از بیرون مگر وقتی رفرنس صفر شده (اگر رفرنس بالای صفر بود فقط کم کند و واقعا آزاد نکند).
تست: تابع kreftest() را اضافه کردم که در بوت از main.c صدا زده میشود و چند خط printf با رفرنس‌ کانت‌ های مورد انتظار چاپ میکند.

<br><br>
![boot_Screenshot](https://raw.githubusercontent.com/DelaraJ/xv6_OS_Lab_project/main/1.png)
<br>

همانطور که در تصویر خروجی مشخص است، موقع بوت خطوط kreftest: را میبینیم با مقادیر مطابق کامنت‌ های expect ....
<br><br>
## تغییر fork برای COW به‌جای کپی فیزیکی
در uvmcopy (در vm.c)، خط kalloc() + memmove() را حذف کردم و به جای آن همان آدرس فیزیکی والد (pa) را در page table فرزند mappages کردم(با فلگ‌ هایی که PTE_W را خاموش کرده و یک فلگ جدید PTE_COW (یکی از بیت‌ های رزرو‌ شده‌ ی PTE در RISC-V) را روشن میکنند) سپس همین فلگ PTE_W را در page table والد هم خاموش کردم چون والد هم باید از این به بعد روی همین صفحه‌ی مشترک فقط بخواند. در پایان incref(pa) زدم چون الان دو page table به یک صفحه‌ ی فیزیکی اشاره میکنند.
PTE_COW را در riscv.h تعریف کردم.
بنابراین در کل در این بخش uvmcopy دیگر کپی فیزیکی نمیکند، صفحه را shared میکند، PTE_W رو خاموش و PTE_COW رو روشن میکند (هم در فرزند هم در والد)، و incref میزند. uvmunmap هم به‌ جای kfree مستقیم، حالا decref صدا میزند (چون صفحات ممکن است shared باشند).

<br><br>
![boot_Screenshot](https://raw.githubusercontent.com/DelaraJ/xv6_OS_Lab_project/main/2.png)
<br>

در این تصویر میبینیم که با اضافه کردن این بخش ها همچنان کرنل در بوت crash نمیکند.
<br><br>
## گسترش vmfault برای مدیریت COW page fault
اینجا تابع را splitمیکنیم به دو حالت: حالت فعلی (lazy allocation، وقتی صفحه اصلا map نشده) که دست‌ نخورده باقی میماند، و حالت جدید برای store fault روی صفحه ی COW: اگر getref==1 فقط PTE_W را بر میگردانیم (بدون کپی، optimization)، وگرنه صفحه ی جدید میگیریم و کپی میکنیم و decref روی صفحه ی قدیمی میزنیم به این صورت که، صفحه ی فیزیکی قدیم را پیدا میکنیم، یک صفحه ی جدید با kalloc() میگیریم، محتوا را با memmove کپی میکنیم PTE فرزند را آپدیت میکنیم تا به صفحه ی جدید با PTE_W روشن و PTE_COW خاموش اشاره کند، و decref روی صفحه ی قدیمی صدا میزنیم (چون دیگر این فرزند به آن اشاره نمیکند). اگر رفرنس کانت صفحه ی قدیمی از قبل ۱ بوده (یعنی فقط همین پروسه به آن اشاره داشته)، میتوان بجای کپی کردن، فقط PTE_W را روشن کنیم.
<br>
- تست:
یک برنامه‌ی یوزر به نام cowtest.c نوشتم که این موقعیت ها را تست کنم:
1.	 fork میزنیم و فرزند یک متغیر گلوبال/heap  را write میکنیم، سپس باید ببینیم که والد مقدار قدیمی را هنوز می‌بیند و فرزند مقدار جدید را
2.	چند بار پشت سرهم fork میزنیم و هرکدام مقدار متفاوتی روی همان صفحه بنویسند، تا نشان دهیم reference counting  وقتی بیش از دو پروسه به یک صفحه اشاره دارند هم درست کار میکند
3.	یک برنامه که حافظ ‌ی زیادی تخصیص دهد و fork بزند را مینویسیم تا نشان دهیم مصرف حافظ ‌ی فیزیکی واقعا کمتر از قبل COW است
این تست ها را به UPROGS در Makefile نیز اضافه کردم

<br><br>
![cowtest_Screenshot](https://raw.githubusercontent.com/DelaraJ/xv6_OS_Lab_project/main/images/3.png)
<br>

همه تست ها پاس شدند<br><br>

## COW Syscall
sys_pgrefcount(uint64 va) را ساختم که آدرس فیزیکی متناظر با va در پروسه ی caller را پیدا کند و page_ref آن را برگرداند. حالا میتوانیم ببینیم که بعد از fork یک رفرنس کانت بالای ۱ داریم و بعد از write به ۱ برمیگردد. <br>
- تست: در cowtest.c، بعد از fork این syscall را صدا میزنیم و رفرنس کانت را پرینت میکنیم (باید ۲ یا بیشتر باشد)، بعد از write  هم دوباره چاپ پرینت میکنیم (باید به ۱ برگردد چون کپی واقعی گرفته شده).

<br><br>
![cowtest_Screenshot](https://raw.githubusercontent.com/DelaraJ/xv6_OS_Lab_project/main/images/4.png)
<br>

بعد از صدا زدن cowtest, خط های refcount_test هم هستند که رفرنس کانت را قبل و بعد از fork و write چاپ میکند. در تصویر هم قابل مشاهده است که مقادیر چاپ شده ب مقادیر مورد انتظارمان تطابق دارد<br><br>

## باگ در copyout()
بعد از پیاده سازی این بخش میخواستم usertests را هم اجرا کنم تا از اینکه برای بخش دیگری مشکلی ایجاد نکرده باشم مطمئن شوم. اما به این مشکل خوردم:
`test stacktest: runtest: fork error reparent: fork failed twochildren: fork failed forkfork: fork failedfork failed createdelete: fork failed forktest: no fork at all! fork failed in sbrkbasic kernmem: fork failed MAXVAplus: fork failed sbrkfail: no allocation failed; allocate more? sbrkfail: fork failed pipe1: pipe1 oops 3 total 0 pipe1: pipe1 oops 1 truncate2: write returned 1, expected -1 truncate3: fork failed openiput: fork failed`
بعد از بررسی فهمیدم که در باگ در تابعcopyout()  در فایل kernel/vm.c  است یعنی جایی دقیقاً جایی که COW با  syscallهای دیگری مثلread()  تلاقی پیدا میکند.<br>
به صورت دقیق تر به این صورت بود که copyout() وقتی میخواهد داده را از کرنل به بافر یوزر بنویسد در توابعی مثل read()، اول walkaddr()  را صدا میزند. اگر صفحه از قبل map شده باشد (حتی اگر COW و read-only باشد)، walkaddr() مقدار غیر صفر برمی‌گرداند، پس شرط if(pa0 == 0)  که باعث صدا زدن vmfault() میشود اصلا اجرا نمیشود. کد مستقیم میرود سراغ:<br>
`if((*pte & PTE_W) == 0)    return -1;`<br>
که این خط قرار بود فقط صفحات متن read-only واقعی (exec) را رد کند، اما صفحات COW  را هم رد می‌کند بدون اینکه هرگز فرصت vmfault() برای resolve کردن COW را داشته باشند.
این موضوع کل usertests را تحت تاثیر قرار میداد چون بعد از fork()  تمام صفحات داده /bss  (مثل بافر گلوبال در pipe1) به صورت COW به اشتراک گذاشته میشوند پس هر read() که میخواست داده در buf بریزدfail  میشد. تست هم بدون wait() کردن exit میکرد و فرزندی که مینوشد رو پایپ بلاک شده باقی میماند. به این صورت به مرور جای proc table و حافظه فیزیکی پرمیشد تا fork() در همه تست های بعدی fail شود.
<br><br>
![cowtest_Screenshot](https://raw.githubusercontent.com/DelaraJ/xv6_OS_Lab_project/main/images/5.png)
<br>
اما با تغییرات در تابع copyout() در کامیت آخر این مشکل مرتفع شد و همه تست های usertests پاس شدند.

</div>
