/*
 * 进程管理器
 * LPCTSTR ： L => Long , P => Point , C => Const , T => 在Win32环境中， 有一个_T宏 , STR => 字符串
 * DWORD: 全称Double Word,是指注册表的键值，每个word为2个字节的长度，DWORD 双字即为4个字节，每个字节是8位，共32位。
 * 在键值项窗口空白处单击右键，选择"新建"菜单项，可以看到这些键值被细分为：字符串值、二进制值、DWORD值、多字符串值、可扩充字符串值五种类型。
 *
 * */
#include <stdio.h>
#include <pthread.h>
#include <windows.h>

pthread_t threads[10];
HANDLE FcpJobObject;
char *CMD = "E:\\wnmp\\php\\php-cgi.exe -b 127.0.0.1:9000";

void *startProcess() {
    while (1) {
        STARTUPINFO si = {sizeof(si)}; //记录结构体有多大，必须要参数
        PROCESS_INFORMATION pi;    //进程id，进程句柄，线程id，线程句柄存在于这个结构体
//        printf("sub ID: %lld\n", pi.dwProcessId);
        if (0 == CreateProcess(NULL,    // 不在此指定可执行文件的文件名
                               CMD,// 命令行参数
                               NULL,    // 默认进程安全性
                               NULL,    // 默认进程安全性
                               TRUE,    // 指定当前进程内句柄不可以被子进程继承
                /* CREATE_NO_WINDOW：创建无控制台进程；
                 * CREATE_SUSPENDED : 设置进程(的主线程)初始状态为 挂起状态（防止加入作业前进程执行任何代码，待加入作业后唤醒线程即可）作业中的进程生成的新进程会自动成为作业的一部分
                 * CREATE_BREAKAWAY_FROM_JOB ：
                 * */
                               CREATE_NO_WINDOW | CREATE_SUSPENDED |
                               CREATE_BREAKAWAY_FROM_JOB,
                               NULL,    // 使用进程的环境变量
                               NULL,    // 使用本进程的驱动器和目录
                               &si,
                               &pi)) {
            fprintf(stderr, "failed to create process %s", CMD);
            return NULL;
        }

        // 将创建出的进程,加入到作业对象,由作业对象管理进程
        if (!AssignProcessToJobObject(FcpJobObject, pi.hProcess)) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return NULL;
        }

        // 唤醒线程
        if (!ResumeThread(pi.hThread)) {
            TerminateProcess(pi.hProcess, 1); // 立即杀死进程
            CloseHandle(pi.hProcess);// 关闭进程
            CloseHandle(pi.hThread);// 关闭线程
            return NULL;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);// 阻塞，等待进程结束
        CloseHandle(pi.hProcess);// 关闭进程
        CloseHandle(pi.hThread);// 关闭进程的主线程
    }

//    return NULL;
}

// 初始化作业对象
void initJob() {
    // 创建作业对象
    FcpJobObject = CreateJobObject(NULL, NULL);
    if (FcpJobObject == NULL)exit(1);

    // 查询作业信息
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;
    if (!QueryInformationJobObject(FcpJobObject, JobObjectExtendedLimitInformation, &limit, sizeof(limit), NULL)) {
        printf("%s\n","=======");
        CloseHandle(FcpJobObject);
        exit(1);
    }

    limit.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(FcpJobObject, JobObjectExtendedLimitInformation, &limit, sizeof(limit))) {
        CloseHandle(FcpJobObject);
        exit(1);
    }
}

// 程序入口
int main(int argc, char *argv[]) {
    initJob();
    // 多线程执行
    for (int i = 0; i < 5; ++i) {
        /*
         * 参数1: 指向线程标识符的指针，type: pthread_t*
         * 参数2: 用来设置线程属性
         * 参数3 : 线程运行函数的起始地址， type: (void*)(*)(void*)
         * 参数4: 运行函数的参数，type: void *
         */
        pthread_create(&threads[i], NULL, startProcess, NULL);
    }

    // 双击运行隐藏窗口
    // ShowWindow(FindWindow("ConsoleWindowClass", argv[0]), SW_HIDE);
    // 命令行和双击都可以隐藏窗口
    ShowWindow(FindWindow("ConsoleWindowClass", NULL), SW_HIDE);


    // 等待线程结束
    for (int i = 0; i < 5; ++i) {
        pthread_join(threads[i], NULL);
    }


    return 0;
}

