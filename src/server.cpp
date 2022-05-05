#include <boost/date_time/time_zone_base.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <iostream>
#include <vector>
#include <algorithm>

namespace lt = boost::local_time;
namespace pt = boost::posix_time;
namespace ip_ = boost::asio::ip;
using namespace std;

string get_time(lt::tz_database tz_db, string time_zone_name){

	stringstream stream_;
	lt::local_time_facet* facet(new lt::local_time_facet("%a %b %e %T %z %Y"));

	stream_.imbue(locale(locale::classic(), facet));

	const vector<string>& all_timezones = tz_db.region_list();

	for (vector<string>::const_iterator tz = all_timezones.begin(); tz != all_timezones.end(); ++tz){
		lt::time_zone_ptr timeZone = tz_db.time_zone_from_region(*tz);
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

	static pointer create(boost::asio::io_context& io_context, lt::tz_database tz_db){
		return pointer(new tcp_connection(io_context, tz_db));
	}

	ip_::tcp::socket& socket(){
		return socket_;
	}

	void start(){		
		socket_.async_write_some(boost::asio::buffer("Enter the time zone abbreviation, for example: BST\n"),
		 boost::bind(&tcp_connection::handle_write, shared_from_this(),
		  boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
		
		boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(message_), "\r\n", 
		 boost::bind(&tcp_connection::handle_read, shared_from_this(),
		  boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

	}

	private:
		tcp_connection(boost::asio::io_context& io_context, lt::tz_database tz_db) : socket_(io_context)
		{	
			time_zone_db = tz_db;	
		}
		
		void handle_write(const boost::system::error_code& error, size_t bytes_transfered)
		{			
		}

		void handle_read(const boost::system::error_code& error, size_t bytes_transfered){
			if (!error){
				
				clean_message = message_.c_str();
				clean_message.replace(clean_message.find("\r\n"), 64, "");
				clean_message.erase(remove(clean_message.begin(), clean_message.end(), ' '), clean_message.end());
				response = get_time(time_zone_db, clean_message);

				socket_.async_write_some(boost::asio::buffer(response),
				  boost::bind(&tcp_connection::handle_write, shared_from_this(),
			       boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
				
			}else{
				cerr << "Error: " << error.what() << endl;
				socket_.close();
			}
		}

		void handle_close(const boost::system::error_code& error, size_t bytes_transfered){
			socket_.close();
			delete this;
		}

		lt::tz_database time_zone_db;

		string clean_message;

		ip_::tcp::socket socket_;
		string message_;
		string response;
};

class tcp_server
{
	public:

	tcp_server(boost::asio::io_context& io_context, int port) : io_context_(io_context), acceptor_(io_context, ip_::tcp::endpoint(ip_::tcp::v4(), port))
	  {
		tz_db.load_from_file("./date_time_zonespec.csv");
		start_accept();
	  }
	
	private:
	
	void start_accept(){
		tcp_connection::pointer new_connection = tcp_connection::create(io_context_, tz_db);
		acceptor_.async_accept(new_connection->socket(), boost::bind(&tcp_server::handle_accept, this, new_connection,
		 boost::asio::placeholders::error));
	}

	void handle_accept(tcp_connection::pointer new_connection, const boost::system::error_code& error){
		if (!error){
			new_connection->start();
			start_accept();
		}else{
			cout << error.message() << endl;
		}
	}

	lt::tz_database tz_db;
	boost::asio::io_context& io_context_;
	ip_::tcp::acceptor acceptor_;
};


int main(int argc, char* argv[]){

	try{
		if (argc != 2){
			cerr << "The <PORT> parameter is not initalized" << endl;
			return 0;
		}

		boost::asio::io_context io_context;
		tcp_server server(io_context, atoi(argv[1]));
		io_context.run();
	}
	catch (exception& e){
		cerr << e.what() << endl;
	}

	return 0;
}
