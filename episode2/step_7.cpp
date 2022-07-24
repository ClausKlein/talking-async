#include <iostream>
#include <string>

#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::dynamic_buffer;
using asio::ip::tcp;
using namespace asio::experimental::awaitable_operators;
using namespace std::literals::chrono_literals;

constexpr auto use_nothrow_awaitable =
    asio::experimental::as_tuple(asio::use_awaitable);

template<typename Stream>
class message_reader
{
public:
  explicit message_reader(Stream& stream)
      : stream_(stream)
  {
  }

  auto read_message() -> awaitable<std::string>
  {
    auto [e, n] = co_await asio::async_read_until(
        stream_, dynamic_buffer(message_buffer_), '|', use_nothrow_awaitable);

    if (e) {
      co_return std::string();
    }

    std::string message(message_buffer_.substr(0, n));
    message_buffer_.erase(0, n);
    co_return message;
  }

private:
  Stream& stream_;
  std::string message_buffer_;
};

auto session(tcp::socket client) -> awaitable<void>
{
  message_reader<tcp::socket> reader(client);

  for (;;) {
    std::string message = co_await reader.read_message();

    if (!message.empty()) {
      std::cout << "received: " << message << "\n";
    } else {
      co_return;
    }
  }
}

auto listen(tcp::acceptor& acceptor) -> awaitable<void>
{
  for (;;) {
    auto [e, client] = co_await acceptor.async_accept(use_nothrow_awaitable);
    if (e) {
      break;
    }

    auto ex = client.get_executor();
    co_spawn(ex, session(std::move(client)), detached);
  }
}

auto main(int argc, char* argv[]) -> int
{
  try {
    if (argc != 3) {
      std::cerr << "Usage: message_server";
      std::cerr << " <listen_address> <listen_port>\n";
      return 1;
    }

    asio::io_context ctx;

    auto listen_endpoint =
        *tcp::resolver(ctx).resolve(argv[1], argv[2], tcp::resolver::passive);

    tcp::acceptor acceptor(ctx, listen_endpoint);

    co_spawn(ctx, listen(acceptor), detached);

    ctx.run();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }
}
