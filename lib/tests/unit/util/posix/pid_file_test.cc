#include "root_path.hpp"

#include <pxp-agent/util/posix/pid_file.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>

#include <leatherman/file_util/file.hpp>

#include <catch.hpp>

#include <sys/file.h>    // open()
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>       // open() flags
#include <unistd.h>      // close()
#include <sys/wait.h>    // waitpid()
#include <signal.h>      // sigemptyset()

namespace PXPAgent {
namespace Util {

namespace fs = boost::filesystem;
namespace lth_file = leatherman::file_util;

const std::string LOCK_PATH { std::string { PXP_AGENT_ROOT_PATH }
                              + "/lib/tests/resources/test_spool/tmp.lock" };
const std::string PID_DIR { std::string { PXP_AGENT_ROOT_PATH }
                            + "/lib/tests/resources/config" };
const std::string TMP_DIR { std::string { PXP_AGENT_ROOT_PATH }
                            + "/lib/tests/resources/test_spool/tmp_pid" };
const std::string PID_FILE_NAME { "pxp-agent.pid" };

void initializeTmpPIDFile(const std::string dir, const std::string& txt) {
    if (!fs::exists(dir) && !fs::create_directories(dir)) {
        FAIL("failed to create tmp_pid directory");
    }

    std::string pid_file_path { dir + "/" + PIDFile::FILENAME };

    try {
        lth_file::atomic_write_to_file(txt + "\n", pid_file_path);
    } catch (const std::exception&) {
        FAIL("failed to write temporary pid file");
        fs::remove_all(dir);
    }
}

TEST_CASE("PIDFile ctor", "[util]") {
    SECTION("can instantiate") {
        REQUIRE_NOTHROW(PIDFile { PID_DIR });
    }

    SECTION("can instantiate if the directory does not exist") {
        REQUIRE_NOTHROW(PIDFile { PID_DIR + "/foo/bar" });

        fs::remove_all(PID_DIR + "/foo");
    }

    SECTION("cannot instantiate if the directory is a regular file") {
        REQUIRE_THROWS_AS(PIDFile { std::string { PXP_AGENT_ROOT_PATH }
                                    + "/README.md" },
                          PIDFile::Error);
    }

    SECTION("create the directory if necessary") {
        REQUIRE_FALSE(fs::exists(PID_DIR + "/foo/bar"));

        PIDFile p_f { PID_DIR + "/foo/bar" };

        REQUIRE(fs::exists(PID_DIR + "/foo/bar"));

        fs::remove_all(PID_DIR + "/foo");
    }

    SECTION("create the PIDFile") {
        REQUIRE_FALSE(fs::exists(PID_DIR + "/foo/bar/" + PID_FILE_NAME));

        PIDFile p_f { PID_DIR + "/foo/bar" };

        REQUIRE(fs::exists(PID_DIR + "/foo/bar/" + PID_FILE_NAME));

        fs::remove_all(PID_DIR + "/foo");
    }
}

TEST_CASE("PIDFile::isExecuting") {
    fs::remove_all(TMP_DIR);

    SECTION("the file contains the PID of an executing process") {
        auto pid = getpid();
        std::string pid_txt { std::to_string(pid) };
        initializeTmpPIDFile(TMP_DIR, pid_txt);
        PIDFile p_f { TMP_DIR };

        REQUIRE(p_f.isExecuting());
    }

    SECTION("returns false if the file does not exist") {
        if (!fs::create_directories(TMP_DIR)) {
            FAIL("failed to create tmp_pid directory");
        }
        PIDFile p_f { TMP_DIR };

        REQUIRE_FALSE(p_f.isExecuting());
    }

    SECTION("returns false if the file is empty") {
        initializeTmpPIDFile(TMP_DIR, "");
        PIDFile p_f { TMP_DIR };

        REQUIRE_FALSE(p_f.isExecuting());
    }

    SECTION("returns false if the file contains an invalid integer string") {
        initializeTmpPIDFile(TMP_DIR, "spam");
        PIDFile p_f { TMP_DIR };

        REQUIRE_FALSE(p_f.isExecuting());
    }

    fs::remove_all(TMP_DIR);
}

TEST_CASE("PIDFile::read", "[util]") {
    SECTION("can successfully read") {
        pid_t pid { 12340987 };
        initializeTmpPIDFile(TMP_DIR, std::to_string(pid));
        PIDFile p_f { TMP_DIR };

        REQUIRE(p_f.read() == pid);
    }

    fs::remove_all(TMP_DIR);
}

TEST_CASE("PIDFile::write", "[util]") {
    SECTION("successfully writes into the PID file") {
        fs::remove_all(TMP_DIR);
        if (!fs::create_directories(TMP_DIR)) {
            FAIL("failed to create tmp_pid directory");
        }
        PIDFile p_f { TMP_DIR };
        pid_t pid { 424242 };
        p_f.write(pid);
        auto read_pid = p_f.read();

        REQUIRE(pid == read_pid);
    }

    fs::remove_all(TMP_DIR);
}

TEST_CASE("PIDFile::cleanup", "[util]") {
    SECTION("successfully removes the PID file") {
        fs::remove_all(TMP_DIR);
        if (!fs::create_directories(TMP_DIR)) {
            FAIL("failed to create tmp_pid directory");
        }
        PIDFile p_f { TMP_DIR };
        pid_t pid { 353535 };
        p_f.write(pid);
        std::string pid_file_path { TMP_DIR + "/" + PIDFile::FILENAME };

        REQUIRE(lth_file::file_readable(pid_file_path));

        p_f.cleanup();

        REQUIRE_FALSE(lth_file::file_readable(pid_file_path));
    }

    fs::remove_all(TMP_DIR);
}

static void dumbSigHandler(int sig) {}

static void testConcurrentLock(int child_lock_type,
                               int parent_lock_type,
                               bool parent_lock_success) {
    sigset_t block_mask, orig_mask, empty_mask;
    struct sigaction sa;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &block_mask, &orig_mask) == -1) {
        FAIL("Failed to set the signal mask");
    }

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = dumbSigHandler;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        FAIL("Failed to set SIGUSR1 disposition");
    }

    auto child_pid = fork();

    switch (child_pid) {
        case -1:
            FAIL("failed to fork");
        case 0:
            {
                // CHILD: acquire lock, signal parent, wait for signal, exit
                auto child_fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
                PIDFile::lockFile(child_fd, child_lock_type);
                kill(getppid(), SIGUSR1);
                sigemptyset(&empty_mask);
                if (sigsuspend(&empty_mask) == -1 && errno != EINTR) {
                    WARN("error while waiting for suspended parent signal");
                }
                close(child_fd);
                _exit(EXIT_SUCCESS);
            }
        default:
            {
                // PARENT: wait for child signal, acquire lock, signal child
                auto parent_fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
                sigemptyset(&empty_mask);
                if (sigsuspend(&empty_mask) == -1 && errno != EINTR) {
                    // Just warn; we don't want the child hanging
                    WARN("error while waiting for suspended parent signal");
                }
                bool success { false };

                if (parent_lock_success) {
                    // Filtering exception for 'success' in order to
                    // be able to signal child and avoid him hanging
                    try {
                        PIDFile::lockFile(parent_fd, parent_lock_type);
                        success = true;
                    } catch (const PIDFile::Error& e) {
                        // pass
                    }
                } else {
                    try {
                        PIDFile::lockFile(parent_fd, parent_lock_type);
                    } catch (const PIDFile::Error& e) {
                        success = true;
                    }
                }
                kill(child_pid, SIGUSR1);
                waitpid(child_pid, nullptr, 0);
                close(parent_fd);
                REQUIRE(success);
            }
    }

    if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) == -1) {
        WARN("Failed to reset signal mask");
    }
}

TEST_CASE("PIDFile::lockFile", "[util]") {
    SECTION("it can lock a file (read lock)") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);

        REQUIRE_NOTHROW(PIDFile::lockFile(fd, F_RDLCK));
        close(fd);
    }

    SECTION("it can lock a file (write lock)") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);

        REQUIRE_NOTHROW(PIDFile::lockFile(fd, F_WRLCK));
        close(fd);
    }

    SECTION("can get a read lock if the file already has one") {
        REQUIRE_NOTHROW(testConcurrentLock(F_RDLCK, F_RDLCK, true));
    }

    SECTION("cannot get a read lock if a write lock was already acquired") {
        REQUIRE_NOTHROW(testConcurrentLock(F_WRLCK, F_RDLCK, false));
    }

    SECTION("cannot get a write lock if a read lock was already acquired") {
        REQUIRE_NOTHROW(testConcurrentLock(F_RDLCK, F_WRLCK, false));
    }

    SECTION("cannot get a write lock if a write lock was already acquired") {
        REQUIRE_NOTHROW(testConcurrentLock(F_WRLCK, F_WRLCK, false));
    }

    SECTION("locking is idempotent, i.e. we can lock the same open fd "
            "multiple times (read lock)") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);
        PIDFile::lockFile(fd, F_RDLCK);

        REQUIRE_NOTHROW(PIDFile::lockFile(fd, F_RDLCK));
        REQUIRE_NOTHROW(PIDFile::lockFile(fd, F_RDLCK));
        close(fd);
    }
}

TEST_CASE("PIDFile::unlockFile", "[util]") {
    SECTION("it unlock a locked file") {
        auto first_fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (first_fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);
        PIDFile::lockFile(first_fd, F_WRLCK);

        // Unlocking a locked file is always ok
        REQUIRE_NOTHROW(PIDFile::unlockFile(first_fd));

        close(first_fd);
    }

    SECTION("unlocking is idempotent, i.e. we can unlock an unlocked file") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        if (fd == -1) FAIL(std::string { "failed to open " } + LOCK_PATH);

        REQUIRE_NOTHROW(PIDFile::unlockFile(fd));
        REQUIRE_NOTHROW(PIDFile::unlockFile(fd));
        close(fd);
    }
}

static void testLockCheck(int child_lock_type,
                          int parent_check_lock_type,
                          bool parent_check_success) {
    sigset_t block_mask, orig_mask, empty_mask;
    struct sigaction sa;

    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &block_mask, &orig_mask) == -1) {
        FAIL("Failed to set the signal mask");
    }

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = dumbSigHandler;

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        FAIL("Failed to set SIGUSR1 disposition");
    }

    auto child_pid = fork();

    switch (child_pid) {
        case -1:
            FAIL("failed to fork");
        case 0:
            {
                // CHILD: acquire lock, signal parent, wait for signal, exit
                auto child_fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
                PIDFile::lockFile(child_fd, child_lock_type);
                kill(getppid(), SIGUSR1);
                sigemptyset(&empty_mask);
                if (sigsuspend(&empty_mask) == -1 && errno != EINTR) {
                    WARN("error while waiting for suspended parent signal");
                }
                close(child_fd);
                _exit(EXIT_SUCCESS);
            }
        default:
            {
                // PARENT: wait for child signal, check lock status, signal child
                auto parent_fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
                sigemptyset(&empty_mask);
                if (sigsuspend(&empty_mask) == -1 && errno != EINTR) {
                    // Just warn; we don't want the child hanging
                    WARN("error while waiting for suspended parent signal");
                }
                bool outcome { false };

                // Filtering exception for 'outcome' in order to
                // be able to signal child and avoid him hanging
                try {
                    outcome = PIDFile::canLockFile(parent_fd,
                                                   parent_check_lock_type);
                } catch (const PIDFile::Error& e) {
                    // pass
                }

                kill(child_pid, SIGUSR1);
                waitpid(child_pid, nullptr, 0);
                close(parent_fd);
                REQUIRE(outcome == parent_check_success);
            }
    }

    if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) == -1) {
        WARN("Failed to reset signal mask");
    }
}

TEST_CASE("PIDFile::canLockFile", "[util]") {
    SECTION("successfully check read lock on unlocked file") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);

        REQUIRE(PIDFile::canLockFile(fd, F_RDLCK));
        close(fd);
    }

    SECTION("successfully check write lock on unlocked file") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);

        REQUIRE(PIDFile::canLockFile(fd, F_WRLCK));
        close(fd);
    }

    SECTION("successfully check file locked by the same process") {
        auto fd = open(LOCK_PATH.data(), O_RDWR | O_CREAT, 0640);
        PIDFile::lockFile(fd, F_RDLCK);

        REQUIRE(PIDFile::canLockFile(fd, F_RDLCK));
        REQUIRE(PIDFile::canLockFile(fd, F_WRLCK));

        PIDFile::lockFile(fd, F_WRLCK);

        REQUIRE(PIDFile::canLockFile(fd, F_RDLCK));
        REQUIRE(PIDFile::canLockFile(fd, F_WRLCK));
        close(fd);
    }

    SECTION("successfully check read lock on locked file (read)") {
        REQUIRE_NOTHROW(testLockCheck(F_RDLCK, F_RDLCK, true));
    }

    SECTION("successfully check write lock on locked file (read)") {
        REQUIRE_NOTHROW(testLockCheck(F_RDLCK, F_WRLCK, false));
    }

    SECTION("successfully check read lock on locked file (write)") {
        REQUIRE_NOTHROW(testLockCheck(F_WRLCK, F_RDLCK, false));
    }

    SECTION("successfully check write lock on locked file (write)") {
        REQUIRE_NOTHROW(testLockCheck(F_WRLCK, F_WRLCK, false));
    }
}

TEST_CASE("PIDFile::isProcessExecuting", "[util]") {
    SECTION("this process is executing") {
        REQUIRE(PIDFile::isProcessExecuting(getpid()));
    }
}

}  // namespace Util
}  // namespace PXPAgent
