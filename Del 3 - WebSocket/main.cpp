#include <iostream>
#include <restinio/all.hpp>
#include <json_dto/pub.hpp>

namespace rr = restinio::router;
using router_t = rr::express_router_t<>;


struct weather_t {
    weather_t() = default;

	weather_t(
		std::string ID, 
        std::string Date,
        std::string Time,
        std::string Name,
        std::string Lat,
        std::string Lon,
        std::string Temperature,
        std::string Humidity ) 
		:	m_ID{ std::move( ID ) }
		,	m_Date{ std::move( Date ) }
		,	m_Time{ std::move( Time ) }    
		,	m_Name{ std::move( Name ) }    
		,	m_Lat{ std::move( Lat ) }    
		,	m_Lon{ std::move( Lon ) }    
		,	m_Temperature{ std::move( Temperature ) }    
		,	m_Humidity{ std::move( Humidity ) }    
	{}

	template < typename JSON_IO >
	void
	json_io( JSON_IO & io )
	{
		io
			& json_dto::mandatory( "ID", m_ID )
			& json_dto::mandatory( "Date", m_Date )
			& json_dto::mandatory( "Time", m_Time )
			& json_dto::mandatory( "Name", m_Name )
			& json_dto::mandatory( "Lat", m_Lat )
			& json_dto::mandatory( "Lon", m_Lon )
			& json_dto::mandatory( "Temperature", m_Temperature )
			& json_dto::mandatory( "Humidity", m_Humidity );
	}

	std::string m_ID;
	std::string m_Date;
	std::string m_Time;
	std::string m_Name;
	std::string m_Lat;
	std::string m_Lon;
	std::string m_Temperature;
	std::string m_Humidity;
};

using weather_collection_t = std::vector< weather_t >;

class weather_handler_t 
{
public:
    explicit weather_handler_t(weather_collection_t &weathers)
        : m_weathers(weathers) {}

    auto on_weather_list(
		const restinio::request_handle_t &req, rr::route_params_t) const 
		{
        auto resp = init_resp(req->create_response());
        resp.set_body(json_dto::to_json(m_weathers));
        return resp.done();
    }

private:
    weather_collection_t &m_weathers;

	template < typename RESP >
	static RESP
	init_resp( RESP resp )
	{
		resp
			.append_header( "Server", "RESTinio sample server /v.0.6" )
			.append_header_date_field()
			.append_header( "Content-Type", "text/plain; charset=utf-8" );

		return resp;
	}

	template < typename RESP >
	static void
	mark_as_bad_request( RESP & resp )
	{
		resp.header().status_line( restinio::status_bad_request() );
	}
};

auto server_handler(weather_collection_t &weather_collection) {
    auto router = std::make_unique<router_t>();
    auto handler = std::make_shared<weather_handler_t>(std::ref(weather_collection));

    auto by = [&](auto method) {
        using namespace std::placeholders;
        return std::bind(method, handler, _1, _2);
    };

    // Handlers for '/' path.
    router->http_get("/", by(&weather_handler_t::on_weather_list));
    return router;
}

int main() {
    using namespace std::chrono;

    try {
        using traits_t =
            restinio::traits_t<
                restinio::asio_timer_manager_t,
                restinio::single_threaded_ostream_logger_t,
                router_t>;

        weather_collection_t weather_collection {
            // Initial weather data.
            {"1", "20211105", "12:15", "Aarhus N", "13.692", "19.438", "13.1", "70%"}
        };

        restinio::run(
            restinio::on_this_thread<traits_t>()
                .address("localhost")
                .request_handler(server_handler(weather_collection))
                .read_next_http_message_timelimit(10s)
                .write_http_response_timelimit(1s)
                .handle_request_timeout(1s));
    }
    catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
