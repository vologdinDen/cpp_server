#include <algorithm>

#include <boost/asio.hpp>
#include <boost/asio/execution.hpp>
#include <boost/bind/bind.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/time_zone_base.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>
#include <vector>

namespace lt = boost::local_time;
namespace pt = boost::posix_time;
using tcp = boost::asio::ip::tcp;

std::string get_time(const lt::tz_database &tz_db, const std::string& time_zone_name){
	
	const auto& all_timezones = tz_db.region_list();
	for (auto &tz: all_timezones){
		lt::time_zone_ptr timeZone = tz_db.time_zone_from_region(tz);
		if (timeZone->std_zone_abbrev() == time_zone_name || timeZone->dst_zone_abbrev() == time_zone_name){
			std::stringstream stream;
			lt::local_time_facet facet("%a %b %e %T %z %Y");
			stream.imbue(std::locale(std::locale::classic(), &facet));
			stream << lt::local_sec_clock::local_time(timeZone) << std::endl;
			return stream.str();
		}
	}
	return "Input Error: There is no such time zone\n";
}

class tcp_connection : public boost::enable_shared_from_this<tcp_connection>
{
	public:
	
		typedef boost::shared_ptr<tcp_connection> pointer;

		static pointer create(const boost::asio::any_io_executor& executor, const lt::tz_database &tz_db){
			return pointer(new tcp_connection(executor, tz_db));
		}

		tcp::socket& socket(){
			return socket_;
		}

		void start(){		
			boost::asio::async_write(socket_, boost::asio::buffer("Enter the time zone abbreviation, for example: BST\n"),
			 [self=shared_from_this()](auto ec, auto bt){});
			
			boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(message_), "\r\n", 
			 [self=shared_from_this()](auto ec, auto bt){self->handle_read(ec, bt);});
		}

	private:
		const lt::tz_database &time_zone_db_;
		tcp::socket socket_;
		std::string message_;
		std::string response_;

		tcp_connection(boost::asio::any_io_executor executor, const lt::tz_database &tz_db) : time_zone_db_(tz_db), socket_(executor) {
		}
		
		void handle_read(const boost::system::error_code& error, size_t bytes_transfered){
			if (!error){		
				auto clean_message = message_;
				if(auto pos = clean_message.find("\r\n"); pos != std::string::npos){clean_message.resize(pos);}
				clean_message.erase(remove(clean_message.begin(), clean_message.end(), ' '), clean_message.end());
				response_ = get_time(time_zone_db_, clean_message);

				boost::asio::async_write(socket_, boost::asio::buffer(response_),
				 [self=shared_from_this()](auto ec, auto bt){self->handle_close();});
				
			}else{
				std::cerr << "Error: " << error.what() << std::endl;
				handle_close();
			}
		}

		void handle_close(){
			socket_.shutdown(tcp::socket::shutdown_send);
			socket_.close();
		}
};

class tcp_server
{
	public:

		tcp_server(boost::asio::any_io_executor executor, int port) : executor_(executor), acceptor_(executor, tcp::endpoint(tcp::v4(), port)){
			tz_db_.load_from_file("./date_time_zonespec.csv");
			start_accept();
		}
	
		void stop() {
			acceptor_.cancel();
		}
	
	private:
		lt::tz_database tz_db_;
		boost::asio::any_io_executor executor_;
		tcp::acceptor acceptor_;
	
		void start_accept(){
			tcp_connection::pointer new_connection = tcp_connection::create(executor_, tz_db_);
			acceptor_.async_accept(new_connection->socket(), [this, new_connection] (auto ec) { handle_accept(new_connection, ec); });
		}

		void handle_accept(tcp_connection::pointer new_connection, const boost::system::error_code& error){
			if (!error){
				new_connection->start();
				start_accept();
			}else{
				std::cout << error.message() << std::endl;
			}
		}

};

int main(int argc, char* argv[]){
	try{
		if (argc != 2){
			std::cerr << "The <PORT> parameter is not initalized" << std::endl;
			return 0;
		}

		boost::asio::io_context io_context;
		tcp_server server(std::move(io_context.get_executor()), atoi(argv[1]));
		
		boost::asio::signal_set signals(io_context.get_executor(), SIGINT, SIGTERM);
		signals.async_wait([&signals, &io_context](auto er, auto signal){
			server.stop()
		});
		io_context.run();
	} catch (const std::exception& e){
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
