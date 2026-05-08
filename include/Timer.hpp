#ifndef ATOM_TIMER_HPP
#define ATOM_TIMER_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace atom_timer {

class Timer {
public:
  /**
   * @brief Costruttore del timer
   * @param timeout_ms Durata del timeout in millisecondi
   * @param callback Funzione da invocare allo scadere del timeout
   */
  explicit Timer(uint64_t timeout_ms, std::function<void()> callback)
      : timeout_duration_(std::chrono::milliseconds(timeout_ms)),
        callback_(callback), running_(false) {}

  /**
   * @brief Distruttore - ferma il timer se in esecuzione
   */
  ~Timer() { stop(); }

  /**
   * @brief Avvia il timer e gestisce automaticamente il timeout
   */
  void start() {

    stop(); // Ferma eventuali timer precedenti
    running_ = true;
    timer_thread_ = std::thread([this]() {
      std::unique_lock<std::mutex> lock(
          mutex_); // Protegge l'accesso a running_

      cv_.wait_for(lock,
                   timeout_duration_); // Risveglia se running_ diventa false

      if (running_) {
        callback_(); // Esegue la callback se il timer è ancora attivo
      }
    });
  }

  void destroy() { stop(); }

private:
  /**
   * @brief Ferma il timer
   */
  void stop() {
    {
      std::lock_guard<std::mutex> lock(mutex_); // Protegge l'accesso a running_
      running_ = false; // Imposta running_ a false per segnalare al thread di
                        // terminare
    }
    cv_.notify_one(); // Risveglia immediatamente il thread in attesa

    if (timer_thread_.joinable()) {
      timer_thread_.join();
    }
  }

  std::chrono::milliseconds timeout_duration_;
  std::function<void()> callback_;
  std::atomic<bool> running_;
  std::atomic<bool> elapsed_{false};
  std::thread timer_thread_;
  std::mutex mutex_;
  std::condition_variable cv_;
};

} // namespace atom_timer

#endif // ATOM_TIMER_HPP