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


std::string get_time(lt::tz_database tz_db, const std::string& time_zone_name){

	std::stringstream stream_;
	auto facet = std::make_unique<lt::local_time_facet>("%a %b %e %T %z %Y");
	stream_.imbue(std::locale(std::locale::classic(), facet.get()));

	
	const auto& all_timezones = tz_db.region_list();

	for (auto &tz: all_timezones){
		lt::time_zone_ptr timeZone = tz_db.time_zone_from_region(tz);
		if (timeZone->std_zone_abbrev() == time_zone_name || timeZone->dst_zone_abbrev() == time_zone_name){
			lt::local_date_time ldt = lt::local_sec_clock::local_time(timeZone);
			stream_ << ldt;
			return stream_.str() + '\n'; 
		}
	}
	return "Input Error: There is no such time zone\n";
}

class tcp_connection : public boost::enable_shared_from_this<tcp_connection>
{
	public:

		typedef boost::shared_ptr<tcp_connection> pointer;

		static pointer create(boost::asio::any_io_executor& executor_, lt::tz_database tz_db){
			return pointer(new tcp_connection(executor_, tz_db));
		}

		tcp::socket& socket(){
			return socket_;
		}

		void start(){		
			socket_.async_write_some(boost::asio::buffer("Enter the time zone abbreviation, for example: BST\n"),
			 [self=shared_from_this()](auto ec, auto bt){self->handle_write(ec, bt);});
			
			boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(message_), "\r\n", 
			 [self=shared_from_this()](auto ec, auto bt){self->handle_read(ec, bt);});
		}

	private:
		tcp_connection(boost::asio::any_io_executor& executor_, lt::tz_database tz_db) : socket_(executor_){	
			time_zone_db_ = tz_db;	
		}
		
		void handle_write(const boost::system::error_code& error, size_t bytes_transfered){			
		}

		void handle_read(const boost::system::error_code& error, size_t bytes_transfered){
			if (!error){
				
				clean_message_ = message_;
				if(auto pos = clean_message_.find("\r\n"); pos != std::string::npos){clean_message_.resize(pos);}
				clean_message_.erase(remove(clean_message_.begin(), clean_message_.end(), ' '), clean_message_.end());
				response_ = get_time(time_zone_db_, clean_message_);

				socket_.async_write_some(boost::asio::buffer(response_),
				 [self=shared_from_this()](auto ec, auto bt){self->handle_close(ec, bt);});
				
			}else{
				std::cerr << "Error: " << error.what() << std::endl;
				socket_.close();
			}
		}

		void handle_close(const boost::system::error_code& error, size_t bytes_transfered){
			socket_.shutdown(tcp::socket::shutdown_send);
			socket_.close();
		}

		lt::tz_database time_zone_db_;

		std::string clean_message_;

		tcp::socket socket_;
		std::string message_;
		std::string response_;
};

class tcp_server
{
	public:

		tcp_server(boost::asio::any_io_executor executor, int port) : acceptor_(executor, tcp::endpoint(tcp::v4(), port)){
			executor_ = executor;
			tz_db_.load_from_file("./date_time_zonespec.csv");
			start_accept();
		}
	
	private:
	
		void start_accept(){
			tcp_connection::pointer new_connection = tcp_connection::create(executor_, tz_db_);
			acceptor_.async_accept(new_connection->socket(), boost::bind(&tcp_server::handle_accept, this, new_connection,
			boost::asio::placeholders::error));
		}

		void handle_accept(tcp_connection::pointer new_connection, const boost::system::error_code& error){
			if (!error){
				new_connection->start();
				start_accept();
			}else{
				std::cout << error.message() << std::endl;
			}
		}

		lt::tz_database tz_db_;
		boost::asio::any_io_executor executor_;
		tcp::acceptor acceptor_;
};


void handler(const boost::system::error_code& error, int signal_number){
	if (!error){
		std::cerr << signal_number << std::endl;
	}
}

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
			io_context.stop();
			signals.clear();
		});
		io_context.run();

		
	}
	catch (std::exception& e){
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
