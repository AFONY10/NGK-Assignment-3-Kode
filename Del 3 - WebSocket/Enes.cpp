#include <iostream>
#include <vector>
#include <restinio/all.hpp>
#include <string>
#include <json_dto/pub.hpp>
#include <restinio/websocket/websocket.hpp>

namespace rr = restinio::router;
using router_t = rr::express_router_t<>;

//For WebSocket
namespace rws = restinio::websocket::basic;
using traits_t =
restinio::traits_t<
restinio::asio_timer_manager_t,
restinio::single_threaded_ostream_logger_t,
router_t >;
using ws_registry_t = std::map< std::uint64_t, rws::ws_handle_t >;

struct place_t
{
	place_t() = default;

	place_t(
		std::string PlaceName,
		double Lat,
		double Lon)
		:  m_PlaceName{ std::move( PlaceName )}
		,  m_Lat{ std::move( Lat )}
		,  m_Lon{ std::move( Lon )}
	{}

	template < typename JSON_IO > 
	void
	json_io(JSON_IO & io)
	{
		io
			& json_dto::mandatory("PlaceName", m_PlaceName )
			& json_dto::mandatory("Lat", m_Lat )
			& json_dto::mandatory("Lon", m_Lon );
	}

	std::string m_PlaceName;
	double m_Lat;
	double m_Lon;
};


struct weatherStation_t
{
	weatherStation_t() = default;

	weatherStation_t(
		std::string ID,
		std::string Date,
		std::string Time,
		place_t place,
		float Temperature,
		int Humidity )
		:	m_ID{ std::move( ID ) }
		,	m_Date{ std::move( Date ) }
		,   m_Time{ std::move( Time ) }
		,   m_place{ std::move( place ) }
		,   m_Temperature{ std::move( Temperature )}
		,   m_Humidity{ std::move( Humidity )}
	{}

	template < typename JSON_IO >
	void
	json_io( JSON_IO & io )
	{
		io
			& json_dto::mandatory( "ID", m_ID )
			& json_dto::mandatory( "Date", m_Date )
			& json_dto::mandatory( "Time", m_Time )
			& json_dto::mandatory( "Place", m_place)
			& json_dto::mandatory( "Temperature", m_Temperature )
			& json_dto::mandatory( "Humidity", m_Humidity );
	}

	std::string m_ID;
	std::string m_Date;
	std::string m_Time;
	place_t m_place;
	float m_Temperature;
	int m_Humidity;
};

using weatherStation_collection_t = std::vector< weatherStation_t >;



class weatherStations_handler_t
{
public :
	explicit weatherStations_handler_t( weatherStation_collection_t & weatherStations )
		:	m_weatherStations( weatherStations )
	{}

	weatherStations_handler_t( const weatherStations_handler_t & ) = delete;
	weatherStations_handler_t( weatherStations_handler_t && ) = delete;

	auto on_weatherStations_list( const restinio::request_handle_t& req, rr::route_params_t ) const
	{
		auto resp = init_resp( req->create_response() );

		resp.set_body(json_dto::to_json(m_weatherStations));

		/*resp.set_body(
			"weatherStation collection (weatherStation count: " + std::to_string( m_weatherStations.size() ) + ")\n" );

		for( std::size_t i = 0; i < m_weatherStations.size(); ++i )
		{
			resp.append_body( std::to_string( i + 1 ) + ". " );
			const auto & b = m_weatherStations[ i ];
			resp.append_body(
				"Date: " + b.m_Date + 
				" ID: " + b.m_ID + 
				" Time: " + b.m_Time + 
				" PlaceName: " + b.m_place.m_PlaceName +
				" Lat: " + std::to_string(b.m_place.m_Lat) + 
				" Lon " + std::to_string(b.m_place.m_Lon) +
				" Temp: " + std::to_string(b.m_Temperature) + 
				" Humidity: " + std::to_string(b.m_Humidity) + "\n");
		}*/

		return resp.done();
	}

	auto on_weatherStation_get( const restinio::request_handle_t& req, rr::route_params_t params )
	{
		const auto StationNum = restinio::cast_to< std::uint32_t >( params[ "StationNum" ] );

		auto resp = init_resp( req->create_response() );

		if( 0 != StationNum && StationNum <= m_weatherStations.size() )
		{
			resp.set_body(json_dto::to_json(m_weatherStations));
		}
		else
		{
			resp.set_body(
				"No weatherStation with #" + std::to_string( StationNum ) + "\n" );
		}

		return resp.done();
	}


	auto on_three_get(const restinio::request_handle_t& req, rr::route_params_t params)
	{
		auto resp = init_resp(req -> create_response() );
		try
		{
			std::vector<weatherStation_t> threeStations;

			int i = 0;
			for (auto iter = m_weatherStations.rbegin(); iter != m_weatherStations.rend() && (i != 3); ++iter, ++i)
			{
				threeStations.push_back(*iter);
				std::cout<< i << " - " << iter->m_ID << std::endl;
			}
			resp.set_body(json_dto::to_json(threeStations));
		}
		catch(const std::exception& threeStations)
		{
			mark_as_bad_request( resp );
		}

		return resp.done();
		
	}

	auto on_Date_get( const restinio::request_handle_t& req, rr::route_params_t params )			
	{
		auto resp = init_resp( req->create_response() );
		try
		{
			std::vector<weatherStation_t> weather_date;
			auto Date = restinio::utils::unescape_percent_encoding(params[ "Date" ]);

			for( std::size_t i = 0; i < m_weatherStations.size(); ++i )
			{
				const auto & b = m_weatherStations[ i ];
				if( Date == b.m_Date )
				{
					weather_date.push_back(b);
				}
			}

			resp.set_body( json_dto::to_json(weather_date));
		}
		catch( const std::exception & )
		{
			mark_as_bad_request( resp );
		}

		return resp.done();
	}

	auto on_ID_get(
		const restinio::request_handle_t& req, rr::route_params_t params )			
	{
		auto resp = init_resp( req->create_response() );
		try
		{
			std::vector<weatherStation_t> weather_id;
			auto ID = restinio::utils::unescape_percent_encoding( params[ "ID" ] );

			for( std::size_t i = 0; i < m_weatherStations.size(); ++i )
			{
				const auto & b = m_weatherStations[ i ];
				if( ID == b.m_ID )
				{
					weather_id.push_back(b);
				}
			}
			resp.set_body( json_dto::to_json(weather_id) );
		}
		catch( const std::exception & )
		{
			mark_as_bad_request( resp );
		}

		return resp.done();
	}

	auto on_new_weatherStation(const restinio::request_handle_t& req, rr::route_params_t )
	{
		auto resp = init_resp( req->create_response() );

		try
		{
			m_weatherStations.emplace_back(
				json_dto::from_json< weatherStation_t >( req->body() ));
			
		}
		catch( const std::exception & /*ex*/ )
		{
			mark_as_bad_request( resp );
		}

		return resp.done();
	}

	auto on_weatherStation_update( const restinio::request_handle_t& req, rr::route_params_t params )
	{
		const auto ID = restinio::cast_to< std::uint32_t >( params[ "ID" ] );

		auto resp = init_resp( req->create_response() );

		try
		{
			auto b = json_dto::from_json< weatherStation_t >( req->body() );

			if( 0 != ID && ID <= m_weatherStations.size() )
			{
				m_weatherStations[ ID - 1 ] = b;
			}
			else
			{
				mark_as_bad_request( resp );
				resp.set_body( "No weatherStation with #" + std::to_string( ID ) + "\n" );
			}
		}
		catch( const std::exception & /*ex*/ )
		{
			mark_as_bad_request( resp );
		}

		return resp.done();
	}

	auto on_weatherStation_delete(
		const restinio::request_handle_t& req, rr::route_params_t params )
	{
		const auto StationNum = restinio::cast_to< std::uint32_t >( params[ "StationNum" ] );

		auto resp = init_resp( req->create_response() );

		if( 0 != StationNum && StationNum <= m_weatherStations.size() )
		{
			resp.set_body(json_dto::to_json(m_weatherStations));

			m_weatherStations.erase( m_weatherStations.begin() + ( StationNum - 1 ) );
		}
		else
		{
			resp.set_body(
				"No weatherStation with #" + std::to_string( StationNum ) + "\n" );
		}

		return resp.done();
	}


auto on_live_update(const restinio::request_handle_t& req,
rr::route_params_t params)
{
if (restinio::http_connection_header_t::upgrade ==
req->header().connection())
{
auto wsh = rws::upgrade<traits_t>(
*req, rws::activation_t::immediate,
[this](auto wsh, auto m)
{
if (rws::opcode_t::text_frame == m->opcode() ||
rws::opcode_t::binary_frame == m->opcode() ||
rws::opcode_t::continuation_frame == m->opcode())
{
wsh->send_message(*m);
}
else if (rws::opcode_t::ping_frame == m->opcode())
{
auto resp = *m;
resp.set_opcode(rws::opcode_t::pong_frame);
wsh->send_message(resp);
}
else if (rws::opcode_t::connection_close_frame == m->opcode())
{
m_registry.erase(wsh->connection_id());
}
});
m_registry.emplace(wsh->connection_id(), wsh);
init_resp(req->create_response()).done();
return restinio::request_accepted();
}
return restinio::request_rejected();
}


auto options(restinio::request_handle_t req,
  	restinio::router::route_params_t)
	{
		const auto methods = "OPTIONS, GET, POST, PATCH, DElETE, PUT";
		auto resp = init_resp(req->create_response());
		resp.append_header(restinio::http_field::access_control_allow_methods,methods);
		resp.append_header(restinio::http_field::access_control_allow_headers, "content-type");
		resp.append_header(restinio::http_field::access_control_max_age, "86400");
		return resp.done();
	} 

private :
	weatherStation_collection_t & m_weatherStations;
	ws_registry_t m_registry;

void sendMessage(std::string message)
	{
		for(auto [k, v] : m_registry)
			v->send_message(rws::final_frame, rws::opcode_t::text_frame, message);
	}

	template < typename RESP >
	static RESP
	init_resp( RESP resp )
	{
		resp
			.append_header( "Server", "RESTinio sample server /v.0.6" )
			.append_header_date_field()
			.append_header( "Content-Type", "application/json" )
			.append_header(restinio::http_field::access_control_allow_origin, "*");
		return resp;
	}

	template < typename RESP >
	static void
	mark_as_bad_request( RESP & resp )
	{
		resp.header().status_line( restinio::status_bad_request() );
	}
};

auto server_handler( weatherStation_collection_t & weatherStation_collection )
{
	auto router = std::make_unique< router_t >();
	auto handler = std::make_shared< weatherStations_handler_t >( std::ref(weatherStation_collection) );

	auto by = [&]( auto method ) {
		using namespace std::placeholders;
		return std::bind( method, handler, _1, _2 );
	};

	auto method_not_allowed = []( const auto & req, auto ) {
			return req->create_response( restinio::status_method_not_allowed() )
					.connection_close()
					.done();
		};

	router->http_get("/chat",by(&weatherStations_handler_t::on_live_update));


	router->add_handler(restinio::http_method_options(),"/",by(&weatherStations_handler_t::options));
	router->add_handler(restinio::http_method_options(), R"(/:StationNum(\d+))", by(&weatherStations_handler_t::options));
	router->add_handler(restinio::http_method_options(), "/three", by(&weatherStations_handler_t::options));
	router->add_handler(restinio::http_method_options(), "/Date/:Date", by(&weatherStations_handler_t::options));



	// Handlers for '/:weatherid' path.
	router->http_get( "/", by( &weatherStations_handler_t::on_weatherStations_list ) );
	router->http_post( "/", by( &weatherStations_handler_t::on_new_weatherStation ) );
	router->http_put( "/", by( &weatherStations_handler_t::on_weatherStation_update ) );

	// Disable all other methods for '/'.
	router->add_handler(
			restinio::router::none_of_methods(
					restinio::http_method_get(), 
					restinio::http_method_post(),
					restinio::http_method_put() ),
			"/", method_not_allowed );

	// Handler for '/' path.
	router->http_get( "/id/:ID", by( &weatherStations_handler_t::on_ID_get ) );

	router->http_get( "/date/:Date", by( &weatherStations_handler_t::on_Date_get ) );

	router->http_get( "/three", by( &weatherStations_handler_t::on_three_get ) );

	router->http_put( "/id/:ID", by( &weatherStations_handler_t::on_weatherStation_update));

	router->add_handler(restinio::http_method_options(),"/id/:ID",by(&weatherStations_handler_t::options));

	// Disable all other methods for '/author/:author'.
	router->add_handler(
			restinio::router::none_of_methods( restinio::http_method_get() ),
			"/id/:ID", method_not_allowed );

	// Handlers for '/:StationNum' path.
	router->http_get(
			R"(/:StationNum(\d+))",
			by( &weatherStations_handler_t::on_weatherStation_get ) );
	router->http_put(
			R"(/:StationNum(\d+))",
			by( &weatherStations_handler_t::on_weatherStation_update ) );
	router->http_delete(
			R"(/:StationNum(\d+))",
			by( &weatherStations_handler_t::on_weatherStation_delete ) );

	// Disable all other methods for '/:StationNum'.
	router->add_handler(
			restinio::router::none_of_methods(
					restinio::http_method_get(),
					restinio::http_method_put(),
					restinio::http_method_post(),
					restinio::http_method_delete() ),
			R"(/:StationNum(\d+))", method_not_allowed );
	
	return router;
}

int main()
{
	using namespace std::chrono;

	try
	{
		using traits_t =
			restinio::traits_t<
				restinio::asio_timer_manager_t,
				restinio::single_threaded_ostream_logger_t,
				router_t >;

		weatherStation_collection_t weatherStation_collection{
			{ "1", "240423" ,"12:15", {"Aalborg",455, 324} , 22 , 50 }
		};
		

		restinio::run(
			restinio::on_this_thread< traits_t >()
				.address( "localhost" )
				.request_handler( server_handler( weatherStation_collection ) )
				.read_next_http_message_timelimit( 10s )
				.write_http_response_timelimit( 1s )
				.handle_request_timeout( 1s ) );
	}
		catch( const std::exception & ex )
		{
			std::cerr << "Error: " << ex.what() << std::endl;
			return 1;
		}

	return 0;
	
}
