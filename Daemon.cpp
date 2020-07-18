#include <QtCore/QCoreApplication>

#ifdef Q_OS_LINUX
#include <unistd.h>
#include <syslog.h>
#else
#include <QTimer>
#include <future>
#endif

#ifdef Q_OS_LINUX
int pidFilehandle;

void signal_handler(int sig) {
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        syslog(LOG_INFO, "Sigterm received, exit...");        
        QCoreApplication::exit(0);
        close(pidFilehandle);
        break;
    }
}

void daemonize(const char* rundir, const char* pidfile) {
    int pid, sid, i;
    char str[10];
    struct sigaction newSigAction;
    sigset_t newSigSet;

    /* Check if parent process id is set */
    if (getppid() == 1) {
        /* PPID exists, therefore we are already a daemon */
        return;
    }

    /* Set signal mask - signals we want to block */
    sigemptyset(&newSigSet);
    sigaddset(&newSigSet, SIGCHLD);  /* ignore child - i.e. we don't need to wait for it */
    sigaddset(&newSigSet, SIGTSTP);  /* ignore Tty stop signals */
    sigaddset(&newSigSet, SIGTTOU);  /* ignore Tty background writes */
    sigaddset(&newSigSet, SIGTTIN);  /* ignore Tty background reads */
    sigprocmask(SIG_BLOCK, &newSigSet, NULL);   /* Block the above specified signals */

    /* Set up a signal handler */
    newSigAction.sa_handler = signal_handler;
    sigemptyset(&newSigAction.sa_mask);
    newSigAction.sa_flags = 0;

    /* Signals to handle */
    sigaction(SIGHUP, &newSigAction, NULL);     /* catch hangup signal */
    sigaction(SIGTERM, &newSigAction, NULL);    /* catch term signal */
    sigaction(SIGINT, &newSigAction, NULL);     /* catch interrupt signal */


    /* Fork*/
    pid = fork();

    if (pid < 0) {
        /* Could not fork */
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        /* Child created ok, so exit parent process */
        //printf("Child process created: %d\n", pid);
        exit(EXIT_SUCCESS);
    }

    // A process inherits its working directory from its parent. This could be
    // on a mounted filesystem, which means that the running daemon would
    // prevent this filesystem from being unmounted. Changing to the root
    // directory avoids this problem.
    // chdir(rundir);

    /* Child continues */

    umask(027); /* Set file permissions 750 */

    /* Get a new process group */
    sid = setsid();

    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    /* close all descriptors */
    for (i = getdtablesize(); i >= 0; --i) {
        close(i);
    }

    /* Route I/O connections */

    /* Open STDIN */
    i = open("/dev/null", O_RDWR);

    /* STDOUT */
    dup(i);

    /* STDERR */
    dup(i);

    /* Ensure only one copy */
    pidFilehandle = open(pidfile, O_RDWR | O_CREAT, 0600);

    if (pidFilehandle == -1) {
        /* Couldn't open lock file */
        syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pidfile);
        exit(EXIT_FAILURE);
    }

    /* Try to lock file */
    if (lockf(pidFilehandle, F_TLOCK, 0) == -1) {
        /* Couldn't get lock on lock file */
        syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pidfile);
        exit(EXIT_FAILURE);
    }

    /* Get and format PID */
    sprintf(str, "%d\n", getpid());

    /* write pid to lockfile */
    write(pidFilehandle, str, strlen(str));
}
#endif

int main(int argc, char* argv[])
{ 
#ifdef Q_OS_LINUX

    std::string mode;
    if (argc > 1) {
        mode = argv[1];
    }

    if (mode.find("--console") == std::string::npos) {
        setlogmask(LOG_UPTO(LOG_INFO));
        openlog("daemon", LOG_CONS | LOG_PERROR, LOG_USER);
        syslog(LOG_INFO, "Daemon starting up");

        const char* daemonpid = "daemon.pid";
        const char* daemonpath = "/";
        daemonize(daemonpath, daemonpid);

        syslog(LOG_INFO, "Daemon running");
    }
    else {
        // not daemon start
    }

    QCoreApplication app(argc, argv);
    // Start app
    return app.exec();
    
#else
    QCoreApplication app(argc, argv);
    // App start, no daemon on Windows
    auto f = std::async(std::launch::async, [&] {
        std::getchar();
        QCoreApplication::exit(0);
    });
    int res = app.exec();
    f.wait();
    return res;
#endif
}
