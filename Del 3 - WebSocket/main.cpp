#include <iostream>
#include <restinio/all.hpp>
#include <json_dto/pub.hpp>
#include <vector>
#include <restinio/websocket/websocket.hpp>

#include <fmt/format.h>

namespace rr = restinio::router;
using router_t = rr::express_router_t<>;

// To handle WebSocket
namespace rws = restinio::websocket::basic;
using ws_registry_t = std::map<std::uint64_t, rws::ws_handle_t>;
using traits_t = restinio::traits_t<restinio::asio_timer_manager_t, restinio::single_threaded_ostream_logger_t, router_t>;

// Implementation of struct weatherStation_t
struct weatherStation_t
{
    weatherStation_t() = default;

    // Constructor for weatherStation_t with members
    weatherStation_t(
        std::string ID,
        std::string Date,
        std::string Time,
		// Place is incorperated here, which gives more simple code
        std::string PlaceName,
        std::string Lat,
        std::string Lon,
        float Temperature,
        int Humidity)
        : m_ID{std::move(ID)},
          m_Date{std::move(Date)},
          m_Time{std::move(Time)},
          m_PlaceName{std::move(PlaceName)},
          m_Lat{std::move(Lat)},
          m_Lon{std::move(Lon)},
          m_Temperature{Temperature},
          m_Humidity{Humidity}
    {}

    // JSON I/O funktion to work with json_dto
    template <typename JSON_IO>
    void
    json_io(JSON_IO &io)
    {
        io
            & json_dto::mandatory("ID", m_ID)
            & json_dto::mandatory("Date", m_Date)
            & json_dto::mandatory("Time", m_Time)
            & json_dto::mandatory("PlaceName", m_PlaceName)
            & json_dto::mandatory("Lat", m_Lat)
            & json_dto::mandatory("Lon", m_Lon)
            & json_dto::mandatory("Temperature", m_Temperature)
            & json_dto::mandatory("Humidity in %", m_Humidity);
    }

    // Members of struct weatherStation_t
    std::string m_ID;
    std::string m_Date;
    std::string m_Time;
    std::string m_PlaceName;
    std::string m_Lat;
    std::string m_Lon;
    float m_Temperature;
    int m_Humidity;
};

// Definition of vector for weatherStation_t
using weatherStation_collection_t = std::vector<weatherStation_t>;

// Class to handle weatherStation
class weatherStation_handler_t
{
public:
    explicit weatherStation_handler_t(weatherStation_collection_t &weatherStation)
        : m_weatherStation(weatherStation)
    {}
	
	weatherStation_handler_t( const weatherStation_handler_t & ) = delete;
	weatherStation_handler_t( weatherStation_handler_t && ) = delete;

    // Handler-function til at behandle en forespørgsel for weatherStation data (Opgave 1)
	auto on_weatherStation_list(
		const restinio::request_handle_t &req, rr::route_params_t) const 
		{
			auto resp = init_resp(req->create_response());

			// JSON-formateret svar.
    		resp.set_body(json_dto::to_json(m_weatherStation));
			return resp.done();
    	}
	// Handler-function to handle HTTP POST-requests for adding new weather data (Opgave 2.1)
    auto on_weatherStation_addNew(const restinio::request_handle_t &req, rr::route_params_t)
    {
		auto resp = init_resp(req->create_response());

		try
		{
		  // Analyzes JSON-data from requests and adds onto stack
		  m_weatherStation.emplace_back(json_dto::from_json<weatherStation_t>(req->body()));

		  // Sends message to WebSocket-clients, added for Delopgave3
		  sendMessage("POST: id =" + json_dto::from_json<weatherStation_t>(req->body()).m_ID);
		}
		catch (const std::exception &)
		{
		  mark_as_bad_request(resp);
		}

		return resp.done();
    }

	// Handler-funktion for at håndtere HTTP GET-anmodninger til "/three". Returnerer de seneste tre vejrposter. (Opgave 2.2)
	auto on_weatherStation_getThree(const restinio::request_handle_t &req, rr::route_params_t params)
	{
		auto resp = init_resp(req->create_response());
		
		try
		{
			std::vector<weatherStation_t> weatherStation_three;
            
			int i=0;

			// Itererer gennem de seneste tre vejrposter.
			for (auto iter = m_weatherStation.rbegin(); iter != m_weatherStation.rend() && (i !=3); ++iter, ++i) 
			{
			  weatherStation_three.push_back(*iter);
			  std::cout << i << " - " << iter-> m_ID << std::endl;
			}
			resp.set_body(json_dto::to_json(weatherStation_three));
		}
		catch( const std::exception &)
			{
				mark_as_bad_request( resp );
			}
		
		return resp.done();
	}

	// Handler-funktion for at håndtere HTTP GET-anmodninger til "/Date/:Date". Returnerer vejrdata for en bestemt dato. (Opgave 2.2)
	auto on_weatherStation_getDate(const restinio::request_handle_t& req, rr::route_params_t params )
	{
		auto resp = init_resp( req->create_response() );
		try
		{
			std::vector<weatherStation_t> weatherStation_date;

			// Henter dato-parameteren fra anmodningen.
			auto Date = restinio::utils::unescape_percent_encoding( params[ "Date" ] );
			
			// Filtrer vejrdata baseret på den angivne dato.
			for( std::size_t i=0; i < m_weatherStation.size(); ++i)
			{
				const auto & b = m_weatherStation[i];
				if ( Date == b.m_Date )
				{
					weatherStation_date.push_back(b);
				}
			}
			resp.set_body(json_dto::to_json(weatherStation_date));
		}
		catch( const std::exception & )
		{
			mark_as_bad_request( resp );
		}	
		return resp.done();
	}

	// Handler-function for handling HTTP PUT-Requests for updating existing data (Opgave 2.3)
	auto on_weatherStation_addUpdate(
		const restinio::request_handle_t& req, rr::route_params_t params )
	{
		const auto ID = restinio::cast_to< std::uint32_t >( params[ "ID" ] );

		auto resp = init_resp( req->create_response() );

		try
		{
			// Getting data from the request and opdates the existing data based on ID
			auto b = json_dto::from_json< weatherStation_t >( req->body() );
			
			if (0 != ID && ID <= m_weatherStation.size())
			{
				m_weatherStation[ID - 1] = b;
			}
			else
			{
				mark_as_bad_request(resp);
			}
		}
		catch (const std::exception & /*ex*/)
		{
		mark_as_bad_request(resp);
		}

		return resp.done();
	}

	// Handler-funktion to handle WebSocket-updates (Opgave 3)
	auto on_weatherStation_liveUpdate(const restinio::request_handle_t &req, rr::route_params_t params)
    {
        if (restinio::http_connection_header_t::upgrade==req ->header().connection() )
        {
			// Upgrading connection to WebSocket.
            auto wsh = rws::upgrade<traits_t>(*req, rws::activation_t::immediate, [this] (auto wsh, auto m)
            {
                if (rws::opcode_t::text_frame==m->opcode()||rws::opcode_t::binary_frame == m->opcode() ||rws::opcode_t::continuation_frame == m->opcode())
                {
                    wsh ->send_message(*m);
                }
                else if (rws::opcode_t::ping_frame==m ->opcode() )
                {
					// Answering ping-messages with pong-messages.
                    auto resp=*m;
                    resp.set_opcode(rws::opcode_t::pong_frame);
                    wsh ->send_message(resp);
                }
                else if (rws::opcode_t::connection_close_frame== m ->opcode() )
                {
					// Removing WebSocket-connection from register when shutdown
                    m_registry.erase(wsh ->connection_id() );
                }
            });

		// Adding WebSocket-handle to register.
        m_registry.emplace(wsh -> connection_id(), wsh);

		// Initializing and sending HTTP-respons without body.
        init_resp(req ->create_response() ).done();

		// Signals accepted request
        return restinio::request_accepted();
        }

		// Signals denied request
        return restinio::request_rejected();
    }

	// Handler-function for handling HTTP OPTIONS-requests.
	auto weatherStation_options(restinio::request_handle_t req, restinio::router::route_params_t)
	{
		// Replying on HTTP OPTIONS-requests with CORS-headers.
		const auto methods="OPTIONS, GET, POST, PUT, PATCH, DELETE";
		auto resp=init_resp(req ->create_response() );
		resp.append_header(restinio::http_field::access_control_allow_methods,methods);
		resp.append_header(restinio::http_field::access_control_allow_headers,"content-type");
		resp.append_header(restinio::http_field::access_control_max_age, "86400");
		return resp.done();
	}

	// Handler-function for handling HTTP DELETE-requests to delete weatherdata.
	auto on_weatherStation_delete(
		const restinio::request_handle_t& req, rr::route_params_t params )
	{
		auto resp = init_resp( req->create_response() );
        const auto ID = restinio::cast_to<std::uint32_t>(params["ID"] );
        
        try 
        {
			if (0 != ID && ID <= m_weatherStation.size())
			{
				// Deleting data based on ID
				m_weatherStation.erase(m_weatherStation.begin() + (ID - 1));
			}
			
		}
        catch(const std::exception & /*ex*/)
        {
            mark_as_bad_request(resp);
        }
        return resp.done();
    }
    
private:
    weatherStation_collection_t &m_weatherStation;

    // Initializing respons with different headers
    template <typename RESP>
    static RESP

    init_resp(RESP resp)
    {
        resp
            .append_header("Server", "RESTinio sample server /v.0.6")
            .append_header_date_field()
            .append_header("Content-Type", "text/plain; charset=utf-8")
			.append_header(restinio::http_field::access_control_allow_origin, "*"); // added line for WebSocket

        return resp;
    }

    // Function to mark request as bad request
    template <typename RESP>
    static void
    mark_as_bad_request(RESP &resp)
    {
        resp.header().status_line(restinio::status_bad_request());
    }

	// Registry for WebSocket to store all the subscribed clients
    ws_registry_t   m_registry;

	// Send message to all connected WebSocket-clients.
    void sendMessage(std::string message)
    {
        for (auto [k, v] : m_registry)
            v -> send_message(rws::final_frame, rws::opcode_t::text_frame, message);
    }
};

// Function to handle server data
auto server_handler(weatherStation_collection_t &weatherStation_collection)
{
    auto router = std::make_unique<router_t>();
    auto handler = std::make_shared<weatherStation_handler_t>(std::ref(weatherStation_collection));

    auto by = [&](auto method) {
        using namespace std::placeholders;
        return std::bind(method, handler, _1, _2);
    };

	auto method_not_allowed = []( const auto & req, auto ) {
			return req->create_response( restinio::status_method_not_allowed() )
					.connection_close()
					.done();
		};
        

    // Handler for '/' path
    router->http_get("/", by(&weatherStation_handler_t::on_weatherStation_list));
	router->http_post( "/", by( &weatherStation_handler_t::on_weatherStation_addNew ) );
	router->http_put( "/", by( &weatherStation_handler_t::on_weatherStation_addUpdate ) );
	router->add_handler(restinio::http_method_options(), "/", by(&weatherStation_handler_t::weatherStation_options)); // Routing for options (CORS)

	// Handlers for '/three' path
	router->http_get("/three", by(&weatherStation_handler_t::on_weatherStation_getThree ) );
	router->add_handler(restinio::http_method_options(), "/three", by(&weatherStation_handler_t::weatherStation_options));

	// Handlers for '/date/:date' path.
	router->http_get( "/Date/:Date", by( &weatherStation_handler_t::on_weatherStation_getDate ) );
	router->add_handler(restinio::http_method_options(), "/Date/:Date", by(&weatherStation_handler_t::weatherStation_options));

	// Handlers for '/id/:ID' path
	router->http_put( "/id/:ID", by( &weatherStation_handler_t::on_weatherStation_addUpdate));
	router->http_put( R"(/:ID(\d+))", by( &weatherStation_handler_t::on_weatherStation_addUpdate)); // added for client interaction
	router->add_handler(restinio::http_method_options(), R"(/:ID(\d+))", by(&weatherStation_handler_t::weatherStation_options)); // Enables CORS for ID routing

	// Handler for WebSocket
    router->http_get("/chat", by(&weatherStation_handler_t::on_weatherStation_liveUpdate)); // Routing for WebSocket

	// Handler for delete
    router->http_delete(R"(/:ID(\d+))", by(&weatherStation_handler_t::on_weatherStation_delete));

	
	// Disable all other methods for '/'.
	router->add_handler(
			restinio::router::none_of_methods(
					restinio::http_method_get(), restinio::http_method_post(), restinio::http_method_delete(), restinio::http_method_options()),
			"/", method_not_allowed );


    return router;
}

// Main-function
int main()
{
    using namespace std::chrono;

    try
    {
        // Defining traits for restinio server
        using traits_t =
            restinio::traits_t<
                restinio::asio_timer_manager_t,
                restinio::single_threaded_ostream_logger_t,
                router_t>;

        // Initial weather data
        weatherStation_collection_t weatherStation_collection{
            {"1", "20231207", "12:15", "Aarhus N", "13.692", "19.438", 13.1, 70}
        };

        // Run restinio server with initialised traits and configuration
        restinio::run(
            restinio::on_this_thread<traits_t>()
                .address("localhost")
                .request_handler(server_handler(weatherStation_collection))
                .read_next_http_message_timelimit(10s)
                .write_http_response_timelimit(1s)
                .handle_request_timeout(1s));
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
