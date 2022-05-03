#include <boost/date_time/time_zone_base.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <iostream>
#include <vector>

using namespace boost::local_time;
using namespace boost::posix_time;
using namespace boost::asio;
using ip::tcp;
using namespace std;

local_date_time get_time(tz_database tz_db, string time_zone_name){

	const vector<string>& all_timezones = tz_db.region_list();
	ptime pt(second_clock::universal_time());

	for (vector<string>::const_iterator tz = all_timezones.begin(); tz != all_timezones.end(); ++tz){
		time_zone_ptr timeZone = tz_db.time_zone_from_region(*tz);
		if (timeZone->dst_zone_abbrev() == time_zone_name){
			local_date_time ldt = local_sec_clock::local_time(timeZone);
			return ldt; 
		}
	}
	cout << "Input Error: There is no such time zone" << endl;
	
}

class tcp_connection : public boost::enable_shared_from_this<tcp_connection>
{
	public:

	typedef boost::shared_ptr<tcp_connection> pointer;

	static pointer create(boost::asio::io_context& io_context, tz_database tz_db){
		return pointer(new tcp_connection(io_context, tz_db));
	}

	tcp::socket& socket(){
		return socket_;
	}

	void start(){
		socket_.async_read_some(boost::asio::buffer(data, max_length),
		  boost::bind(&tcp_connection::handle_read, shared_from_this(),
		    boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));


		// boost::asio::streambuf buf;
		// boost::asio::async_read_until(socket_, buf, '\n', 
		//  boost::bind(&tcp_connection::handle_read, shared_from_this(),
		//   boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}

	private:
		tcp_connection(boost::asio::io_context& io_context, tz_database tz_db) : socket_(io_context)
		{
			local_time_facet* facet(new local_time_facet("%a %b %e %T %z %Y"));
			ss.imbue(locale(locale::classic(), facet));
			time_zone_db = tz_db;	
		}
		
		void handle_write(const boost::system::error_code& error, size_t bytes_transfered)
		{
			
		}

		void handle_read(const boost::system::error_code& error, size_t bytes_transfered){
			if (!error){
				
				cout << data << endl;
				local_date_time ldt = get_time(time_zone_db, data);
				cout << ldt << endl;
				
				ss << ldt << endl;

				socket_.async_write_some(boost::asio::buffer(ss.str()),
				  boost::bind(&tcp_connection::handle_write, shared_from_this(),
			      boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
				
				// memset(&(data[0]), 0, max_length);
				
			}else{
				cerr << "Error: " << error.what() << endl;
				socket_.close();
			}
		}

		tz_database time_zone_db;
		stringstream ss;

		tcp::socket socket_;
		string message_;
		enum {max_length = 3};
		char data[max_length];
};

class tcp_server
{
	public:

	tcp_server(boost::asio::io_context& io_context) : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), 1234))
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
		}
		start_accept();
	}

	tz_database tz_db;
	stringstream ss;

	boost::asio::io_context& io_context_;
	tcp::acceptor acceptor_;
};


int main(){

	try{
		boost::asio::io_context io_context;
		tcp_server server(io_context);
		io_context.run();
	}
	catch (exception& e){
		cerr << e.what() << endl;
	}

	return 0;
}
