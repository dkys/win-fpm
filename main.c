/*
 * 进程管理器
 * LPCTSTR ： L => Long , P => Point , C => Const , T => 在Win32环境中， 有一个_T宏 , STR => 字符串
 * DWORD: 全称Double Word,是指注册表的键值，每个word为2个字节的长度，DWORD 双字即为4个字节，每个字节是8位，共32位。
 * 在键值项窗口空白处单击右键，选择"新建"菜单项，可以看到这些键值被细分为：字符串值、二进制值、DWORD值、多字符串值、可扩充字符串值五种类型。
 *
 * */
#include <stdio.h>
#include <pthread.h>
#include <getopt.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>

#pragma comment(lib, "ws2_32.lib")
#define MAX_PROCESSES 1024
pthread_t threads[10];
HANDLE FcpJobObject;
int number = 3;
char *ip = "127.0.0.1";
int port = 9000;
char *path;
struct sockaddr_in listen_addr;
SOCKET listen_fd;
static struct option long_options[] = {
        {"help",    no_argument,       NULL, 'h'},
        {"version", no_argument,       NULL, 'v'},
        {"number",  required_argument, NULL, 'n'},
        {"ip",      required_argument, NULL, 'i'},
        {"port",    required_argument, NULL, 'p'},
//        {"user",    required_argument, NULL, 'u'},
//        {"group",   required_argument, NULL, 'g'},
//        {"root",    required_argument, NULL, 'r'},
        {NULL,      0,                 NULL, 0}
};


static void usage(FILE *where) {
    fprintf(stdout, ""
                    "Usage: xxfpm path [-n number] [-i ip] [-p port]\n"
                    "Manage FastCGI processes.\n"
                    "\n"
                    " -h, --help    output usage information and exit\n"
                    " -v, --version output version information and exit\n"
                    " -n, --number  number of processes to keep\n"
                    " -i, --ip      ip address to bind\n"
                    " -p, --port    port to bind, default is 8000\n"
//                    " -u, --user    start processes using specified linux user\n"
//                    " -r, --root    change root direcotry for the processes"
    );
    exit(where == stderr ? 1 : 0);
}

static void showVersion() {
    fprintf(stdout, ""
                    "xxfpm Revision: 0.01\n"
                    "FastCGI Process Manager\n"
                    "Copyright 2021 qq673675158\n"
                    "Compiled on %s\n", __DATE__
    );
    exit(0);
}

void listenAndBind() {
    //初始化WSA
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    /*
     * WSAStartup:  启动windows异步套接字(只有启动了该程序 才能进一步调用windows socket)
     * 参数sockVersion: socket 版本
     * 参数wsaData : 指向WSADATA数据结构的指针，该数据结构将接收Windows套接字实现的详细信息
     * */
    if (WSAStartup(sockVersion, &wsaData) != 0) {
        exit(EXIT_FAILURE);
    }

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fprintf(stderr, "failed socket %llu\n", listen_fd);
        exit(EXIT_FAILURE);
    }
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = inet_addr(ip);
    listen_addr.sin_port = htons(port);
    if (-1 == bind(listen_fd, (struct sockaddr *) &listen_addr, sizeof(struct sockaddr_in))) {
        fprintf(stderr, "failed bind ip %s,port %d\n", ip, port);
        exit(EXIT_FAILURE);
    }
    listen(listen_fd, MAX_PROCESSES);
}

/**
 * dwFlags
 * STARTF_USESIZE					//使用dwXSize 和dwYSize 成员
 * STARTF_USESHOWWINDOW				//使用wShowWindow 成员
 * STARTF_USEPOSITION				//使用dwX 和dwY 成员
 * STARTF_USECOUNTCHARS             //使用dwXCountChars 和dwYCount Chars 成员
 * STARTF_USEFILLATTRIBUTE			//使用dwFillAttribute 成员
 * STARTF_USESTDHANDLES				//使用hStdInput 、hStdOutput 和hStdError 成员
 * STARTF_RUN_FULLSCREEN			//强制在x86 计算机上运行的控制台应用程序以全屏幕方式启动运行
 *
 */
void *startProcess() {
    while (1) {
        PROCESS_INFORMATION pi;    //进程id，进程句柄，线程id，线程句柄存在于这个结构体
        STARTUPINFO si = {sizeof(si)}; //记录结构体有多大，必须要参数
        si.cb = sizeof(STARTUPINFO);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE; // 隐藏窗口
        si.hStdInput = (HANDLE) listen_fd; // 用于设定供控制台输入和输出用的缓存的句柄。按照默认设置，hStdInput 用于标识键盘缓存，hStdOutput 和hStdError用于标识控制台窗口的缓存
        si.hStdOutput = INVALID_HANDLE_VALUE;
        si.hStdError = INVALID_HANDLE_VALUE;

        if (0 == CreateProcess(NULL,    // 不在此指定可执行文件的文件名
                               path,// 命令行参数
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
            fprintf(stderr, "failed to create process %s", path);
            break;
        }

        // 将创建出的进程,加入到作业对象,由作业对象管理进程
        if (!AssignProcessToJobObject(FcpJobObject, pi.hProcess)) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            break;
        }

        // 唤醒线程
        if (!ResumeThread(pi.hThread)) {
            TerminateProcess(pi.hProcess, 1); // 立即杀死进程
            CloseHandle(pi.hProcess);// 关闭进程
            CloseHandle(pi.hThread);// 关闭线程
            break;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);// 阻塞，等待进程结束
        CloseHandle(pi.hProcess);// 关闭进程
        CloseHandle(pi.hThread);// 关闭进程的主线程
    }
}

// 初始化作业对象
void initJob() {
    // 创建作业对象
    FcpJobObject = CreateJobObject(NULL, NULL);
    if (FcpJobObject == NULL)exit(1);

    // 查询作业信息
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit;
    if (!QueryInformationJobObject(FcpJobObject, JobObjectExtendedLimitInformation, &limit, sizeof(limit), NULL)) {
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
    if (argc == 1) usage(stderr);
    int opt;
    char opts[] = "hvn:i:p:u::g::r::";
    int idx = 0;
    path = argv[1];
    while ((opt = getopt_long(argc, argv, opts, long_options, &idx)) != EOF) {
        switch (opt) {
            case 'h':
                usage(stdout);
                break;
            case 'v':
                showVersion();
                break;
            case 'n':
                number = strtol(optarg, NULL, 10);
                if (number == 0) usage(stderr);
                if (number > MAX_PROCESSES) number = MAX_PROCESSES;
                break;
            case 'i':
                ip = optarg;
                break;
            case 'p':
                port = strtol(optarg, NULL, 10);
                if (port == 0) usage(stderr);
                break;
            default:
                printf("default");
        }
    }
    printf("number => %d ip => %s port => %d path => %s\n", number, ip, port, path);
    initJob();
    listenAndBind();

    // 多线程执行
    for (int i = 0; i < number; ++i) {
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

