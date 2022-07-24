#include <array>
#include <chrono>
#include <iostream>
#include <memory>

#include <asio.hpp>
#include <asio/experimental/as_tuple.hpp>
#include <asio/experimental/awaitable_operators.hpp>

using asio::awaitable;
using asio::buffer;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
namespace this_coro = asio::this_coro;
using namespace asio::experimental::awaitable_operators;
using std::chrono::steady_clock;
using namespace std::literals::chrono_literals;

constexpr auto use_nothrow_awaitable =
    asio::experimental::as_tuple(asio::use_awaitable);

auto timeout(steady_clock::duration duration) -> awaitable<void>
{
  asio::steady_timer timer(co_await this_coro::executor);
  timer.expires_after(duration);
  co_await timer.async_wait(use_nothrow_awaitable);
}

auto transfer(tcp::socket& from, tcp::socket& to) -> awaitable<void>
{
  std::array<char, 1024> data {};

  for (;;) {
    auto result1 =
        co_await (from.async_read_some(buffer(data), use_nothrow_awaitable)
                  || timeout(5s));

    if (result1.index() == 1) {
      co_return;  // timed out
    }

    auto [e1, n1] = std::get<0>(result1);
    if (e1) {
      break;
    }

    auto result2 =
        co_await (async_write(to, buffer(data, n1), use_nothrow_awaitable)
                  || timeout(1s));

    if (result2.index() == 1) {
      co_return;  // timed out
    }

    auto [e2, n2] = std::get<0>(result2);
    if (e2) {
      break;
    }
  }
}

auto proxy(tcp::socket client, tcp::endpoint target) -> awaitable<void>
{
  tcp::socket server(client.get_executor());

  auto [e] = co_await server.async_connect(target, use_nothrow_awaitable);
  if (!e) {
    co_await (transfer(client, server) || transfer(server, client));
  }
}

auto listen(tcp::acceptor& acceptor, tcp::endpoint target) -> awaitable<void>
{
  for (;;) {
    auto [e, client] = co_await acceptor.async_accept(use_nothrow_awaitable);
    if (e) {
      break;
    }

    auto ex = client.get_executor();
    co_spawn(ex, proxy(std::move(client), target), detached);
  }
}

auto main(int argc, char* argv[]) -> int
{
  try {
    if (argc != 5) {
      std::cerr << "Usage: proxy";
      std::cerr << " <listen_address> <listen_port>";
      std::cerr << " <target_address> <target_port>\n";
      return 1;
    }

    asio::io_context ctx;

    auto listen_endpoint =
        *tcp::resolver(ctx).resolve(argv[1], argv[2], tcp::resolver::passive);

    auto target_endpoint = *tcp::resolver(ctx).resolve(argv[3], argv[4]);

    tcp::acceptor acceptor(ctx, listen_endpoint);

    co_spawn(ctx, listen(acceptor, target_endpoint), detached);

    ctx.run();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }
}
