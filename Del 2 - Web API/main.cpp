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
        std::string locationName,
        std::string Lat,
        std::string Lon,
        float Temperature,
        int Humidity)
        : m_ID{std::move(ID)},
          m_Date{std::move(Date)},
          m_Time{std::move(Time)},
          m_locationName{std::move(locationName)},
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
            & json_dto::mandatory("LocationName", m_locationName)
            & json_dto::mandatory("Lat", m_Lat)
            & json_dto::mandatory("Lon", m_Lon)
            & json_dto::mandatory("Temperature", m_Temperature)
            & json_dto::mandatory("Humidity in %", m_Humidity);
    }

    // Medlemmer af structen weatherStation_t
    std::string m_ID;
    std::string m_Date;
    std::string m_Time;
    std::string m_locationName;
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

    // Handler funktion til at behandle en forespørgsel for weatherStation data
	auto on_weatherStation_list(
		const restinio::request_handle_t &req, rr::route_params_t) const 
		{
			auto resp = init_resp(req->create_response());

			// JSON-formateret svar.
    		resp.set_body(json_dto::to_json(m_weatherStation));
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

    // Funktion til at markere en forespørgsel som dårlig
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
            {"1", "20211105", "12:15", "Aarhus N", "13.692", "19.438", 13.1, 70}
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
