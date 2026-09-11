#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

// Функция-обработчик операции приготовления хот-дога
using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Order : public std::enable_shared_from_this<Order> {
public:
    Order(net::io_context& io, int id, std::shared_ptr<Sausage> sausage, std::shared_ptr<Bread> bread, HotDogHandler handler, std::shared_ptr<GasCooker> gas_cooker)
        : strand_{net::make_strand(io)}
        , id_{id}
        , sausage_{std::move(sausage)}
        , bread_{std::move(bread)}
        , handler_{std::move(handler)}
        , gas_cooker_{std::move(gas_cooker)}
        , bread_timer_{io}
        , sausage_timer_{io} {
    }

    // Запускает асинхронное выполнение заказа
    void Execute() {
        net::dispatch(strand_, [self = shared_from_this()] {
            try {
                self->bread_->StartBake(*self->gas_cooker_, [self] {
                    net::dispatch(self->strand_, [self] {
                        self->OnBreadStarted();
                    });
                });
                self->sausage_->StartFry(*self->gas_cooker_, [self] {
                    net::dispatch(self->strand_, [self] {
                        self->OnSausageStarted();
                    });
                });
            } catch (...) {
                self->Deliver(std::current_exception());
            }
        });
    }

private:
    void OnBreadStarted() {
        if (delivered_) {
            return;
        }

        bread_timer_.expires_after(HotDog::MIN_BREAD_COOK_DURATION);
        bread_timer_.async_wait(net::bind_executor(strand_, [self = shared_from_this()](sys::error_code ec) {
            if (ec || self->delivered_) {
                return;
            }

            try {
                self->bread_->StopBaking();
                self->TryPack();
            } catch (...) {
                self->Deliver(std::current_exception());
            }
        }));
    }

    void OnSausageStarted() {
        if (delivered_) {
            return;
        }

        sausage_timer_.expires_after(HotDog::MIN_SAUSAGE_COOK_DURATION);
        sausage_timer_.async_wait(net::bind_executor(strand_, [self = shared_from_this()](sys::error_code ec) {
            if (ec || self->delivered_) {
                return;
            }

            try {
                self->sausage_->StopFry();
                self->TryPack();
            } catch (...) {
                self->Deliver(std::current_exception());
            }
        }));
    }

    [[nodiscard]] bool IsReadyToPack() const {
        return bread_->IsCooked() && sausage_->IsCooked();
    }

    void TryPack() {
        if (!delivered_ && IsReadyToPack()) {
            Deliver({});
        }
    }

    void Deliver(std::exception_ptr error) {
        if (delivered_) {
            return;
        }
        
        if (error) {
            delivered_ = true;
            handler_(Result<HotDog>{std::move(error)});
        } else {
            Result<HotDog> result{HotDog{id_, sausage_, bread_}};
            delivered_ = true;
            handler_(std::move(result));
        }
    }

    net::strand<net::io_context::executor_type> strand_;
    int id_;
    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<Bread> bread_;
    HotDogHandler handler_;
    std::shared_ptr<GasCooker> gas_cooker_;
    net::steady_timer bread_timer_;
    net::steady_timer sausage_timer_;
    bool delivered_ = false;
};

// Класс "Кафетерий". Готовит хот-доги
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io},
          strand_{net::make_strand(io_)} {
    }

    // Асинхронно готовит хот-дог и вызывает handler, как только хот-дог будет готов.
    // Этот метод может быть вызван из произвольного потока
    void OrderHotDog(HotDogHandler handler) {
        net::post(strand_, [handler = std::move(handler), this]() {
            int id = NextHotDogId();
            std::shared_ptr<Sausage> sausage = store_.GetSausage();
            std::shared_ptr<Bread> bread = store_.GetBread();
            
            net::post(io_, [this, id, handler = std::move(handler), sausage = std::move(sausage), bread = std::move(bread)]() {
                auto order = std::make_shared<Order>(
                    io_,
                    id,
                    sausage,
                    bread,
                    std::move(handler),
                    gas_cooker_
                );
                order->Execute();
            });
        });
    }

private:

    int NextHotDogId() {
        return ++hot_dog_id_;
    }

    int hot_dog_id_ = 0;
    net::io_context& io_;
    net::strand<net::io_context::executor_type> strand_;
    // Используется для создания ингредиентов хот-дога
    Store store_;
    // Газовая плита. По условию задачи в кафетерии есть только одна газовая плита на 8 горелок
    // Используйте её для приготовления ингредиентов хот-дога.
    // Плита создаётся с помощью make_shared, так как GasCooker унаследован от
    // enable_shared_from_this.
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
