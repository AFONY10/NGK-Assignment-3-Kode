#include <iostream>
#include <restinio/all.hpp>
#include <json_dto/pub.hpp>
#include <vector>

namespace rr = restinio::router;
using router_t = rr::express_router_t<>;


// Definition af structen weatherStation_t
struct weatherStation_t
{
    weatherStation_t() = default;

    // Konstruktør til weatherStation_t med medlemmerne
    weatherStation_t(
        std::string ID,
        std::string Date,
        std::string Time,
		// Place er inkorporeret heri, giver mere simpel kode
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

    // JSON I/O funktion til at arbejde med json_dto
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

    // Medlemmer af structen weatherStation_t
    std::string m_ID;
    std::string m_Date;
    std::string m_Time;
    std::string m_PlaceName;
    std::string m_Lat;
    std::string m_Lon;
    float m_Temperature;
    int m_Humidity;
};

// Definition af vektor af weatherStation_t
using weatherStation_collection_t = std::vector<weatherStation_t>;

// Klasse til at håndtere weatherStation
class weatherStation_handler_t
{
public:
    explicit weatherStation_handler_t(weatherStation_collection_t &weatherStation)
        : m_weatherStation(weatherStation)
    {}

    // Handler funktion til at behandle en forespørgsel for weatherStation data (Opgave 1)
	auto on_weatherStation_list(
		const restinio::request_handle_t &req, rr::route_params_t) const 
		{
			auto resp = init_resp(req->create_response());

			// JSON-formateret svar.
    		resp.set_body(json_dto::to_json(m_weatherStation));
			return resp.done();
    	}
	// Handler-funktion for at håndtere HTTP POST-anmodninger til at tilføje ny vejrdata. (Opgave 2.1)
    auto on_weatherStation_addNew(const restinio::request_handle_t &req, rr::route_params_t)
    {
		auto resp = init_resp(req->create_response());

		try
		{
		  // Analyserer JSON-data fra anmodningen og tilføjer det til vejrdatasamlingen.
		  m_weatherStation.emplace_back(json_dto::from_json<weatherStation_t>(req->body()));
		  
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

	// Handler-funktion for at håndtere HTTP PUT-anmodninger til at opdatere eksisterende vejrdata (Opgave 2.3)
	auto on_weatherStation_addUpdate(
		const restinio::request_handle_t& req, rr::route_params_t params )
	{
		const auto ID = restinio::cast_to< std::uint32_t >( params[ "ID" ] );

		auto resp = init_resp( req->create_response() );

		try
		{
			// Henter vejrdata fra anmodningen og opdaterer den eksisterende data baseret på ID.
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


	
private:
    weatherStation_collection_t &m_weatherStation;

    // Initialiser respons med de nødvendige headers
    template <typename RESP>
    static RESP
    init_resp(RESP resp)
    {
        resp
            .append_header("Server", "RESTinio sample server /v.0.6")
            .append_header_date_field()
            .append_header("Content-Type", "text/plain; charset=utf-8");

        return resp;
    }

    // Funktion til at markere en forespørgsel som bad request
    template <typename RESP>
    static void
    mark_as_bad_request(RESP &resp)
    {
        resp.header().status_line(restinio::status_bad_request());
    }
};

// Funktion til at oprette serverens håndtering af weatherStation data
auto server_handler(weatherStation_collection_t &weatherStation_collection)
{
    auto router = std::make_unique<router_t>();
    auto handler = std::make_shared<weatherStation_handler_t>(std::ref(weatherStation_collection));

    auto by = [&](auto method) {
        using namespace std::placeholders;
        return std::bind(method, handler, _1, _2);
    };

    // Handler for '/' path
    router->http_get("/", by(&weatherStation_handler_t::on_weatherStation_list));
	router->http_post( "/", by( &weatherStation_handler_t::on_weatherStation_addNew ) );
	router->http_put( "/", by( &weatherStation_handler_t::on_weatherStation_addUpdate ) );

	// Handlers for '/three' path
	router->http_get("/three", by(&weatherStation_handler_t::on_weatherStation_getThree ) );

	// Handlers for '/date/:date' path.
	router->http_get( "/Date/:Date", by( &weatherStation_handler_t::on_weatherStation_getDate ) );

	// Handlers for '/id/:ID' path
	router->http_put( "/id/:ID", by( &weatherStation_handler_t::on_weatherStation_addUpdate));

    return router;
}

// Main funktionen
int main()
{
    using namespace std::chrono;

    try
    {
        // Definerer traits for restinio serveren
        using traits_t =
            restinio::traits_t<
                restinio::asio_timer_manager_t,
                restinio::single_threaded_ostream_logger_t,
                router_t>;

        // Initial weather data
        weatherStation_collection_t weatherStation_collection{
            {"1", "20231207", "12:15", "Aarhus N", "13.692", "19.438", 13.1, 70}
        };

        // Kør restinio serveren med definerede traits og konfiguration
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
