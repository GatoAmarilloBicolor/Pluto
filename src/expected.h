#pragma once

#include <variant>
#include <string>
#include <utility>
#include <type_traits>

namespace pluto {

// Simple Result type for error handling - similar to Rust's Result or C++23's std::expected
template<typename T, typename E = std::string>
class Result {
public:
    using ValueType = T;
    using ErrorType = E;

    // Success constructor
    Result(T&& value) : data_(std::in_place_index<0>, std::move(value)) {}
    Result(const T& value) : data_(std::in_place_index<0>, value) {}

    // Error constructor
    Result(E&& error) : data_(std::in_place_index<1>, std::move(error)) {}
    Result(const E& error) : data_(std::in_place_index<1>, error) {}

    // Check if success
    bool ok() const { return data_.index() == 0; }
    bool is_error() const { return data_.index() == 1; }

    // Get value (throws if error)
    T& value() & { 
        if (is_error()) throw std::runtime_error("Result is error: " + std::get<1>(data_));
        return std::get<0>(data_); 
    }
    const T& value() const & { 
        if (is_error()) throw std::runtime_error("Result is error: " + std::get<1>(data_));
        return std::get<0>(data_); 
    }
    T&& value() && { 
        if (is_error()) throw std::runtime_error("Result is error: " + std::get<1>(data_));
        return std::move(std::get<0>(data_)); 
    }

    // Get error (throws if success)
    E& error() & { 
        if (ok()) throw std::runtime_error("Result is success");
        return std::get<1>(data_); 
    }
    const E& error() const & { 
        if (ok()) throw std::runtime_error("Result is success");
        return std::get<1>(data_); 
    }
    E&& error() && { 
        if (ok()) throw std::runtime_error("Result is success");
        return std::move(std::get<1>(data_)); 
    }

    // Unwrap or return default
    T unwrap_or(T&& default_value) & {
        return ok() ? std::move(std::get<0>(data_)) : std::move(default_value);
    }
    T unwrap_or(const T& default_value) const & {
        return ok() ? std::get<0>(data_) : default_value;
    }

    // Map success value
    template<typename F>
    auto map(F&& f) & -> Result<std::invoke_result_t<F, T&>, E> {
        using U = std::invoke_result_t<F, T&>;
        if (ok()) return Result<U, E>(f(std::get<0>(data_)));
        return Result<U, E>(std::get<1>(data_));
    }
    template<typename F>
    auto map(F&& f) && -> Result<std::invoke_result_t<F, T&&>, E> {
        using U = std::invoke_result_t<F, T&&>;
        if (ok()) return Result<U, E>(f(std::move(std::get<0>(data_))));
        return Result<U, E>(std::move(std::get<1>(data_)));
    }

    // Map error
    template<typename F>
    auto map_error(F&& f) & -> Result<T, std::invoke_result_t<F, E&>> {
        using E2 = std::invoke_result_t<F, E&>;
        if (is_error()) return Result<T, E2>(f(std::get<1>(data_)));
        return Result<T, E2>(std::get<0>(data_));
    }
    template<typename F>
    auto map_error(F&& f) && -> Result<T, std::invoke_result_t<F, E&&>> {
        using E2 = std::invoke_result_t<F, E&&>;
        if (is_error()) return Result<T, E2>(f(std::move(std::get<1>(data_))));
        return Result<T, E2>(std::move(std::get<0>(data_)));
    }

    // And then (chain operations)
    template<typename F>
    auto and_then(F&& f) & -> std::invoke_result_t<F, T&> {
        using U = std::invoke_result_t<F, T&>;
        if (ok()) return f(std::get<0>(data_));
        return U(std::get<1>(data_));
    }
    template<typename F>
    auto and_then(F&& f) && -> std::invoke_result_t<F, T&&> {
        using U = std::invoke_result_t<F, T&&>;
        if (ok()) return f(std::move(std::get<0>(data_)));
        return U(std::move(std::get<1>(data_)));
    }

    // Or else (handle error)
    template<typename F>
    auto or_else(F&& f) & -> Result<T, E> {
        using U = std::invoke_result_t<F, E&>;
        if (is_error()) return f(std::get<1>(data_));
        return *this;
    }
    template<typename F>
    auto or_else(F&& f) && -> Result<T, E> {
        using U = std::invoke_result_t<F, E&&>;
        if (is_error()) return f(std::move(std::get<1>(data_)));
        return std::move(*this);
    }

private:
    std::variant<T, E> data_;
};

// Specialization for void success type
template<typename E>
class Result<void, E> {
public:
    using ValueType = void;
    using ErrorType = E;

    Result() : data_(std::in_place_index<0>) {}
    Result(E&& error) : data_(std::in_place_index<1>, std::move(error)) {}
    Result(const E& error) : data_(std::in_place_index<1>, error) {}

    bool ok() const { return data_.index() == 0; }
    bool is_error() const { return data_.index() == 1; }

    void value() const { 
        if (is_error()) throw std::runtime_error("Result is error: " + std::get<1>(data_)); 
    }
    E& error() & { 
        if (ok()) throw std::runtime_error("Result is success");
        return std::get<1>(data_); 
    }
    const E& error() const & { 
        if (ok()) throw std::runtime_error("Result is success");
        return std::get<1>(data_); 
    }

    template<typename F>
    auto map(F&& f) & -> Result<std::invoke_result_t<F>, E> {
        using U = std::invoke_result_t<F>;
        if (ok()) return Result<U, E>(f());
        return Result<U, E>(std::get<1>(data_));
    }

    template<typename F>
    auto and_then(F&& f) & -> std::invoke_result_t<F> {
        using U = std::invoke_result_t<F>;
        if (ok()) return f();
        return U(std::get<1>(data_));
    }

    template<typename F>
    auto or_else(F&& f) & -> Result<void, E> {
        using U = std::invoke_result_t<F, E&>;
        if (is_error()) return f(std::get<1>(data_));
        return *this;
    }

private:
    std::variant<std::monostate, E> data_;
};

// Helper functions
template<typename T, typename E = std::string>
Result<std::decay_t<T>, std::decay_t<E>> Ok(T&& value) {
    return Result<std::decay_t<T>, std::decay_t<E>>(std::forward<T>(value));
}

template<typename E = std::string>
Result<void, std::decay_t<E>> Ok() {
    return Result<void, std::decay_t<E>>();
}

template<typename T, typename E = std::string>
Result<std::decay_t<T>, std::decay_t<E>> Err(E&& error) {
    return Result<std::decay_t<T>, std::decay_t<E>>(std::forward<E>(error));
}

} // namespace pluto