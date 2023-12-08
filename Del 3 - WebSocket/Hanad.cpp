#include <iostream>
#include <restinio/all.hpp>
#include <json_dto/pub.hpp>
#include <restinio/websocket/websocket.hpp>
#include <fmt/format.h>

namespace rr = restinio::router;
using router_t = rr::express_router_t<>;
namespace rws = restinio::websocket::basic;
using ws_registry_t = std::map<std::uint64_t, rws::ws_handle_t>;
using traits_t = restinio::traits_t<restinio::asio_timer_manager_t, restinio::single_threaded_ostream_logger_t, router_t>;

struct WeatherEntity {
    std::string m_ID;
    std::string m_Date;
    std::string m_Time;
    std::string m_Name;
    std::string m_Lat;
    std::string m_Lon;
    std::string m_Temperature;
    std::string m_Humidity;

    template <typename JSON_IO>
    void json_io(JSON_IO& io) {
        io & json_dto::mandatory("ID", m_ID) & json_dto::mandatory("Date", m_Date) &
            json_dto::mandatory("Time", m_Time) & json_dto::mandatory("Name", m_Name) &
            json_dto::mandatory("Lat", m_Lat) & json_dto::mandatory("Lon", m_Lon) &
            json_dto::mandatory("Temperature", m_Temperature) & json_dto::mandatory("Humidity", m_Humidity);
    }
};

using WeatherCollection = std::vector<WeatherEntity>;

class WeatherHandler {
public:
    explicit WeatherHandler(WeatherCollection& weathers)
        : m_weathers(weathers) {}

    auto on_weather_list(const restinio::request_handle_t& req, rr::route_params_t) const {
        auto resp = init_resp(req->create_response());
        resp.set_body(json_dto::to_json(m_weathers));
        return resp.done();
    }

    auto on_get_three_weather(const restinio::request_handle_t& req, rr::route_params_t params) {
        auto resp = init_resp(req->create_response());
        try {
            std::vector<WeatherEntity> weather_three;

            int i = 0;
            for (auto iter = m_weathers.rbegin(); iter != m_weathers.rend() && (i != 3); ++iter, ++i) {
                weather_three.push_back(*iter);
                std::cout << i << " - " << iter->m_ID << "\n";
            }

            if (!weather_three.empty()) {
                resp.set_body(json_dto::to_json(weather_three));
            } else {
                resp.set_body("No weather data available");
                mark_as_not_found(resp);
            }
        } catch (const std::exception& ex) {
            std::cerr << "Exception caught: " << ex.what() << "\n";
            mark_as_bad_request(resp);
        }

        return resp.done();
    }

    auto on_new_weather(const restinio::request_handle_t& req, rr::route_params_t) {
        auto resp = init_resp(req->create_response());

        try {
            m_weathers.emplace_back(json_dto::from_json<WeatherEntity>(req->body()));
        } catch (const std::exception& ex) {
            std::cerr << "Exception caught: " << ex.what() << "\n";
            mark_as_bad_request(resp);
        }

        return resp.done();
    }
    

    auto on_update_weather(const restinio::request_handle_t& req, rr::route_params_t params) {
        const auto ID = restinio::cast_to<std::uint32_t>(params["ID"]);

        auto resp = init_resp(req->create_response());

        try {
            auto updated_weather = json_dto::from_json<WeatherEntity>(req->body());

            if (0 != ID && ID <= m_weathers.size()) {
                m_weathers[ID - 1] = updated_weather;
            } else {
                resp.set_body("Weather not found");
                mark_as_not_found(resp);
            }
        } catch (const std::exception& ex) {
            std::cerr << "Exception caught: " << ex.what() << "\n";
            mark_as_bad_request(resp);
        }

        return resp.done();
    }

    auto on_delete_weather(const restinio::request_handle_t& req, rr::route_params_t params) {
        const auto ID = restinio::cast_to<std::uint32_t>(params["ID"]);

        auto resp = init_resp(req->create_response());

        if (0 != ID && ID <= m_weathers.size()) {
            m_weathers.erase(m_weathers.begin() + (ID - 1));
            resp.set_body(json_dto::to_json(m_weathers));
        } else {
            resp.set_body("Weather not found");
            mark_as_not_found(resp);
        }

        return resp.done();
    }

    auto on_date_get(const restinio::request_handle_t& req, rr::route_params_t params) {
        auto resp = init_resp(req->create_response());
        try {
            std::vector<WeatherEntity> weather_date;
            auto Date = restinio::utils::unescape_percent_encoding(params["Date"]);

            for (const auto& weather : m_weathers) {
                if (Date == weather.m_Date) {
                    weather_date.push_back(weather);
                }
            }

            if (!weather_date.empty()) {
                resp.set_body(json_dto::to_json(weather_date));
            } else {
                resp.set_body("No weather data available for the specified date");
                mark_as_not_found(resp);
            }
        } catch (const std::exception& ex) {
            std::cerr << "Exception caught: " << ex.what() << "\n";
            mark_as_bad_request(resp);
        }

        return resp.done();
    }

    auto on_live_update(const restinio::request_handle_t& req, rr::route_params_t params) {
        if (restinio::http_connection_header_t::upgrade == req->header().connection()) {
            auto resp = init_resp(req->create_response());

            auto wsh = rws::upgrade<traits_t>(*req, rws::activation_t::immediate, [this](auto wsh, auto m) {
                if (rws::opcode_t::text_frame == m->opcode() || rws::opcode_t::binary_frame == m->opcode() ||
                    rws::opcode_t::continuation_frame == m->opcode()) {
                    wsh->send_message(*m);
                } else if (rws::opcode_t::ping_frame == m->opcode()) {
                    auto resp = *m;
                    resp.set_opcode(rws::opcode_t::pong_frame);
                    wsh->send_message(resp);
                } else if (rws::opcode_t::connection_close_frame == m->opcode()) {
                    m_registry.erase(wsh->connection_id());
                }
            });

            m_registry.emplace(wsh->connection_id(), wsh);
            init_resp(req->create_response()).done();
            return restinio::request_accepted();
        }

        return restinio::request_rejected();
    }

    auto handle_options(const restinio::request_handle_t req, restinio::router::route_params_t) {
        // Definér de tilladte HTTP-metoder for denne ressource.
        const auto allowed_methods = "OPTIONS, GET, POST, PUT, PATCH, DELETE";

        // Opret en HTTP-respons.
        auto resp = init_resp(req->create_response());

        // Tilføj headers til at angive de tilladte metoder og andre CORS-oplysninger.
        resp.append_header(restinio::http_field::access_control_allow_methods, allowed_methods);
        resp.append_header(restinio::http_field::access_control_allow_headers, "content-type");
        resp.append_header(restinio::http_field::access_control_max_age, "86400");

        // Returner responsen.
        return resp.done();
    }

private:
    WeatherCollection& m_weathers;
    ws_registry_t m_registry;

    template <typename RESP>
    static RESP init_resp(RESP resp) {
        resp.append_header("Server", "RESTinio sample server /v.0.6")
            .append_header_date_field()
            .append_header("Content-Type", "text/plain; charset=utf-8")
            .append_header(restinio::http_field::access_control_allow_origin, "*");
        return resp;
    }

    template <typename RESP>
    static void mark_as_bad_request(RESP& resp) {
        resp.header().status_line(restinio::status_bad_request());
    }

    template <typename RESP>
    static void mark_as_not_found(RESP& resp) {
        resp.header().status_line(restinio::status_not_found());
    }

    void sendMessage(const std::string& message) {
        for (const auto& [id, wsh] : m_registry) {
        wsh->send_message(rws::final_frame, rws::opcode_t::text_frame, message);
    }
}
    
};

auto server_handler(WeatherCollection& weather_collection) {
    auto router = std::make_unique<router_t>();
    auto handler = std::make_shared<WeatherHandler>(weather_collection);
    auto by = [&](auto method) {
        using namespace std::placeholders;
        return std::bind(method, handler, _1, _2);
    };

    router->http_get("/", by(&WeatherHandler::on_weather_list));
    router->http_get("/:ID", by(&WeatherHandler::on_get_three_weather));
    router->http_post("/", by(&WeatherHandler::on_new_weather));
    router->http_put("/:ID", by(&WeatherHandler::on_update_weather));
    router->http_delete("/:ID", by(&WeatherHandler::on_delete_weather));
    router->http_get("/date/:Date", by(&WeatherHandler::on_date_get));
    router->http_get("/live_update", by(&WeatherHandler::on_live_update));
    router->add_handler(restinio::http_method_options(), "/.*", by(&WeatherHandler::handle_options));

    return router;
}


int main() {
    using namespace std::chrono;

    try {
        using traits_t =
            restinio::traits_t<restinio::asio_timer_manager_t, restinio::single_threaded_ostream_logger_t, router_t>;

        WeatherCollection weather_collection{
            {"1", "20211105", "12:15", "Aarhus N", "13.692", "19.438", "13.1", "70%"}};

        restinio::run(
            restinio::on_this_thread<traits_t>()
                .address("localhost")
                .request_handler(server_handler(weather_collection))
                .read_next_http_message_timelimit(10s)
                .write_http_response_timelimit(1s)
                .handle_request_timeout(1s));
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
 
 
<!-- <html>
<head>
  <title>Weather Client</title>
  <script src="https://unpkg.com/axios/dist/axios.min.js"></script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/jqueryui/1.12.1/jquery-ui.min.js"></script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/tabulator/3.5.3/js/tabulator.min.js"></script>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/tabulator/3.5.3/css/tabulator.min.css">

  <script type="text/javascript">
    const socket = new WebSocket('ws://localhost:8080/chat');
    socket.addEventListener('open', function (event) {
      socket.send('Hello Server!');
    });
    socket.addEventListener('message', function (message) {
      console.log('Message from server', message.data);
      document.getElementById("update").innerHTML = message.data;
    });
    getData();

    function setTable(data) {
      $("#table").tabulator({
        layout: "fitDataFill",
        height: "311px",
        columns: [
          { title: "ID", field: "ID" },
          { title: "Date", field: "Date" },
          { title: "Time", field: "Time" },
          { title: "Name", field: "Name" },
          { title: "Lat", field: "Lat" },
          { title: "Lon", field: "Lon" },
          { title: "Temperature", field: "Temperature" },
          { title: "Humidity", field: "Humidity" }
        ],
      });

      $("#table").tabulator("setData", data);
    }

    function getData() {
      axios
        .get("http://localhost:8080")
        .then((response) => {
          setTable(response.data);
        })
        .catch((error) => alert("Try again"));
    }

    function sendData() {
      axios
        .post("http://localhost:8080", {
          "ID": document.getElementById("ID").value,
          "Date": document.getElementById("Date").value,
          "Time": document.getElementById("Time").value,
          "Name": document.getElementById("Name").value,
          "Lat": document.getElementById("Lat").value,
          "Lon": document.getElementById("Lon").value,
          "Temperature": document.getElementById("Temperature").value,
          "Humidity": document.getElementById("Humidity").value
        })
        .then((response) => {})
        .catch((error) => alert("Try again"));

      socket.send('ID: ' + document.getElementById("ID").value + ' added.');
    }

    function updateData(idnumber) {
      var url = "http://localhost:8080/" + idnumber;

      axios
        .put(url, {
          "ID": document.getElementById("ID").value,
          "Date": document.getElementById("Date").value,
          "Time": document.getElementById("Time").value,
          "Name": document.getElementById("Name").value,
          "Lat": document.getElementById("Lat").value,
          "Lon": document.getElementById("Lon").value,
          "Temperature": document.getElementById("Temperature").value,
          "Humidity": document.getElementById("Humidity").value
        })
        .then((response) => {})
        .catch((error) => alert("Try again"));

      socket.send('ID: ' + document.getElementById("ID").value + ' updated.');
    }

    function deleteById(idnumber) {
      var url = 'http://localhost:8080/' + idnumber;

      axios
        .delete(url, {
          "ID": document.getElementById("ID").value,
          "Date": document.getElementById("Date").value,
          "Time": document.getElementById("Time").value,
          "Name": document.getElementById("Name").value,
          "Lat": document.getElementById("Lat").value,
          "Lon": document.getElementById("Lon").value,
          "Temperature": document.getElementById("Temperature").value,
          "Humidity": document.getElementById("Humidity").value
        })
        .then(response => {})
        .catch(error => alert('Try again'));

      socket.send('ID: ' + document.getElementById("ID").value + ' deleted.');
    }
  </script>
</head>

<body>
  <h1>Weathers</h1>

  <p>Id<input id="ID" type="text"></p>
  <p>Date<input id="Date" type="text"></p>
  <p>Time<input id="Time" type="text"></p>
  <p>Placename<input id="Name" type="text"></p>
  <p>Lat<input id="Lat" type="text"></p>
  <p>Lon<input id="Lon" type="text"></p>
  <p>Temperature<input id="Temperature" type="text"></p>
  <p>Humidity<input id="Humidity" type="text"></p>

  <input type="button" onclick="getData()" value="Get Data">
  <input type="button" onclick="sendData()" value="Send Data"><br>

  <p>Change Id<input id="lognr" type="text"></p>
  <input type="button" onclick="updateData(document.getElementById('lognr').value)" value="Update Data">
  <input type="button" onclick="deleteById(document.getElementById('lognr').value)" value="Delete Data">

  <br>
  <b>Action:</b>
  <i id="update">*Nothing*</i><br>

  <div id="table"></div>
</body>
</html>
 -->

 <!----------------------------------------------------------------------------- Virkende kode----------------------------------------------------------------------->
 
 <!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Weather Client</title>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/tabulator/3.5.3/css/tabulator.min.css">
  <style>
    body {
      font-family: 'Arial', sans-serif;
      background-color: #f4f4f4;
      margin: 20px;
      color: #333;
    }

    h1 {
      color: #333;
      text-align: center;
    }

    p {
      margin: 10px 0;
    }

    input[type="text"] {
      width: 200px;
      padding: 8px;
      box-sizing: border-box;
    }

    input[type="button"] {
      padding: 8px 16px;
      background-color: #4caf50;
      color: white;
      border: none;
      cursor: pointer;
    }

    input[type="button"]:hover {
      background-color: #45a049;
    }

    #update {
      font-style: italic;
    }

    #table {
      margin-top: 20px;
    }

    /* Color customization */
    #Date, #Time {
      color: #8e44ad; /* Lyse lilla */
    }

    #Name, #Lat, #Lon {
      color: #3498db; /* Lyseblå */
    }

    #Temperature {
      color: #e74c3c; /* Mørkerød */
    }

    #Humidity {
      color: #2ecc71; /* Mørkegrøn */
    }

    #ID {
      color: #f39c12; /* Orange */
    }
  </style>
  <script src="https://unpkg.com/axios/dist/axios.min.js"></script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/jqueryui/1.12.1/jquery-ui.min.js"></script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/tabulator/3.5.3/js/tabulator.min.js"></script>
  <script>
    const socket = new WebSocket('ws://localhost:8080/chat');
    socket.addEventListener('open', function (event) {
      socket.send('Hello Server!');
    });
    socket.addEventListener('message', function (message) {
      console.log('Message from server', message.data);
      document.getElementById("update").innerHTML = message.data;
    });
    getData();

    function setTable(data) {
      $("#table").tabulator({
        layout: "fitDataFill",
        height: "311px",
        columns: [
          { title: "ID", field: "ID" },
          { title: "Date", field: "Date" },
          { title: "Time", field: "Time" },
          { title: "Name", field: "Name" },
          { title: "Lat", field: "Lat" },
          { title: "Lon", field: "Lon" },
          { title: "Temperature", field: "Temperature" },
          { title: "Humidity", field: "Humidity" }
        ],
      });

      $("#table").tabulator("setData", data);
    }

    function getData() {
      axios
        .get("http://localhost:8080")
        .then((response) => {
          setTable(response.data);
        })
        .catch((error) => alert("Try again"));
    }

    function sendData() {
      axios
        .post("http://localhost:8080", {
          "ID": document.getElementById("ID").value,
          "Date": document.getElementById("Date").value,
          "Time": document.getElementById("Time").value,
          "Name": document.getElementById("Name").value,
          "Lat": document.getElementById("Lat").value,
          "Lon": document.getElementById("Lon").value,
          "Temperature": document.getElementById("Temperature").value,
          "Humidity": document.getElementById("Humidity").value
        })
        .then((response) => {})
        .catch((error) => alert("Try again"));

      socket.send('ID: ' + document.getElementById("ID").value + ' added.');
    }

    function updateData(idnumber) {
      var url = "http://localhost:8080/" + idnumber;

      axios
        .put(url, {
          "ID": document.getElementById("ID").value,
          "Date": document.getElementById("Date").value,
          "Time": document.getElementById("Time").value,
          "Name": document.getElementById("Name").value,
          "Lat": document.getElementById("Lat").value,
          "Lon": document.getElementById("Lon").value,
          "Temperature": document.getElementById("Temperature").value,
          "Humidity": document.getElementById("Humidity").value
        })
        .then((response) => {})
        .catch((error) => alert("Try again"));

      socket.send('ID: ' + document.getElementById("ID").value + ' updated.');
    }

    function deleteById(idnumber) {
      var url = 'http://localhost:8080/' + idnumber;

      axios
        .delete(url, {
          "ID": document.getElementById("ID").value,
          "Date": document.getElementById("Date").value,
          "Time": document.getElementById("Time").value,
          "Name": document.getElementById("Name").value,
          "Lat": document.getElementById("Lat").value,
          "Lon": document.getElementById("Lon").value,
          "Temperature": document.getElementById("Temperature").value,
          "Humidity": document.getElementById("Humidity").value
        })
        .then(response => {})
        .catch(error => alert('Try again'));

      socket.send('ID: ' + document.getElementById("ID").value + ' deleted.');
    }
  </script>
</head>

<body>
  <h1>Weathers</h1>

  <p>Id<input id="ID" type="text"></p>
  <p>Date<input id="Date" type="text"></p>
  <p>Time<input id="Time" type="text"></p>
  <p>Placename<input id="Name" type="text"></p>
  <p>Lat<input id="Lat" type="text"></p>
  <p>Lon<input id="Lon" type="text"></p>
  <p>Temperature<input id="Temperature" type="text"></p>
  <p>Humidity<input id="Humidity" type="text"></p>

  <input type="button" onclick="getData()" value="Get Data">
  <input type="button" onclick="sendData()" value="Send Data"><br>

  <p>Change Id<input id="lognr" type="text"></p>
  <input type="button" onclick="updateData(document.getElementById('lognr').value)" value="Update Data">
  <input type="button" onclick="deleteById(document.getElementById('lognr').value)" value="Delete Data">

  <br>
  <b>Action:</b>
  <i id="update">*Nothing*</i><br>

  <div id="table"></div>
</body>
</html>
