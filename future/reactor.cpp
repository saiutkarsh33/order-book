
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <condition_variable>
#include <iostream>
// Future considerations: multiplexing using kqueue, and a thread pool (the reactor pattern)

class ThreadPool {
public:
   
    using Task = std::function<void()>; // callable that takes in no parameters and returns nothing
    explicit ThreadPool(size_t n) {
        for (size_t i = 0; i < n; ++i)
            workers_.emplace_back([this]{ worker(); });
    }
    ~ThreadPool() {
        { std::lock_guard<std::mutex> lock(mtx); 
        stop_ = true; }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }

    template<class F> 
    void post(F&& f) {
        { std::lock_guard<std::mutex> lock(mtx); 
        q_.emplace(std::forward<F>(f)); 
        }
        cv_.notify_one();
    }
private:
void worker() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mtx);

            // Wait until either stop_ is set or there’s something in the queue
            cv_.wait(lock, [this] {
                return stop_ || !q_.empty();
            });

            // Exit condition: stopping and no more tasks left
            if (stop_ && q_.empty()) {
                return;
            }

            // Retrieve the next task
            task = std::move(q_.front());
            q_.pop();
        }

        // ---- Execute task outside the lock ----
        try {
            task();
        } catch (...) {
            // Swallow exceptions to keep worker alive
        }
    }
}
    std::vector<std::thread> workers_;
    std::queue<Task> q_;
    std::mutex mtx; // on the q
    std::condition_variable cv_;
    bool stop_ = false;
};

int main() {

    int kq = kqueue();
    if (kq == -1) { perror("kqueue"); return 1; }

    std::vector<struct kevent> evs(256);

    ThreadPool pool{10};

    // must make the listening fd non blocking bc currently its blocking

    // After accept() by the server docket, mark the client fd non bcloking and add it to the kqueue

    // You register a (ident, filter) subscription (a knote) — e.g. (fd, EVFILT_READ) or (fd, EVFILT_WRITE). Here for the clients you only add EVFILT Read

    while (true) {
        int nev = kevent(kq, nullptr, 0, evs.data(), (int)evs.size(), nullptr); // blocking wait

        for (int i = 0; i < nev; ++i) {
            const auto& ev = evs[i];
            // You can do pool.post the ev, 
            // each ev will give the ready FD
            // then using the FD you must do your io loop again, within the task() logic above

            // One note of consideration: to ensure instrument level concurrency, you need to ensure that there is a lock acquired on the 
            // buy and sell priority queues bc there are potentially multiple threads for one instrument

            // Another note: the "ready queue" in kqueue is not really added in order, something to consider 

            // When fds are cleaned up, the knote is removed the kqueue automatically
            
        }
    }

    close(kq);
    return 0;
}