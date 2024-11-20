#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include "json.hpp"
#include <chrono>
#include <thread>
#include <filesystem>
#include <string>
#include <ctime>
#include <cstdlib>
#include <unordered_map>

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

const string URL = "https://api.razorpay.com/v1/orders";
const string AUTH_HEADER = "SECRET HERE";

const string ORDERS_FOLDER = "orders";

std::string printISTTime(time_t timestamp)
{
    timestamp += 19800; // 5 hours 30 minutes in seconds

    std::tm *istTime = std::gmtime(&timestamp);

    std::ostringstream oss;
    oss << std::put_time(istTime, "%Y-%m-%d %H:%M:%S") << " IST"; 
    return oss.str();                                             
}

void printOrder(const std::string &id)
{
    std::string command = "printcmd.vbs orders/" + id + ".txt";
    int result = system(command.c_str());

    if (result == 0)
    {
        std::cout << "Printed " + id + " \n\n";
    }
    else
    {
        std::cerr << "Failed to print " + id + " \n\n";
    }
}

unordered_map<string, string> productNames = {
    {"li_PBF1pMgK6943Uw", "Schezwan Plain Maggie"},
    {"li_PBF1pNmbNdeScB", "Egg Maggie"},
    {"li_PBF1pNSGr8G43n", "Maggie (plain)"},
    {"li_PBF1pO5JppC5Z6", "Cheese Maggie (plain)"},
    {"li_PBF1pOTuy71ArL", "Vegetable Maggie"},
    {"li_PBF1pP2r8DlTvw", "Peri-Peri Cheese Maggie"},
    {"li_PBF1pPbXhYt78y", "Schezwan Cheese Maggie"},
    {"li_PBF1pQ8ECVaSiH", "Butter Maggie"},
    {"li_PBF1pQjXY8nlaj", "Peri-Peri Plain Maggie"},
    {"li_PBF1pOlBIjyPe6", "Masala Grilled Sandwich"},
    {"li_PBF1pPJonAs3Lr", "Veg Grilled Cheese Sandwich"},
    {"li_PBF1pPs6seKigO", "Cheese Chutney Sandwich"},
    {"li_PBF1pQQnoogOoi", "Veg Grilled Sandwich"},
    {"li_PBF1pN50eCVwxK", "Thakur Ji"},
    {"li_PBF1pR0saECbz1", "Havish Ji"}};

string parseOrders(const string &jsonData)
{
    json orders = json::parse(jsonData);
    string result;

    for (const auto &item : orders)
    {
        int qty = item["qty"];
        int amt = item["amt"];
        string liId = item["liId"];

        // Get product name from liId, or use "Unknown Product" if not found
        string productName = productNames.count(liId) ? productNames[liId] : "Unknown Product";

        // Append formatted order details to the result string
        result += ">> " + productName + " (" + to_string(qty) + ") Rs." + to_string(amt / 100) + "\n";
    }

    return result;
}

size_t WriteCallback(void *contents, size_t size, size_t nmemb, string *data)
{
    data->append((char *)contents, size * nmemb);
    return size * nmemb;
}

void savePaymentToFile(const json &payment)
{
    string id = payment["id"];
    fs::create_directories(ORDERS_FOLDER); // Ensure the folder exists
    string filePath = ORDERS_FOLDER + "/" + id + ".txt";

    // Check if the file already exists
    if (fs::exists(filePath))
    {
        return; // Exit without overwriting
    }

    ofstream file(filePath);
    if (file.is_open())
    {
        string text = "";
        text += "Order ID: " + payment["id"].get<string>() + " \n";
        text += "Ordered at: " + printISTTime(payment["created_at"].get<int>()) + " \n";

        // Add customer name
        if (payment.contains("customer_details") && payment["customer_details"].contains("shipping_address") &&
            payment["customer_details"]["shipping_address"].contains("name"))
        {
            text += "Name: " + payment["customer_details"]["shipping_address"]["name"].get<string>() + " \n";
        }
        else
        {
            text += "Name: Not available \n";
        }

        if (payment.contains("customer_details") && payment["customer_details"].contains("contact"))
        {
            text += "Contact: " + payment["customer_details"]["contact"].get<string>() + " \n";
        }
        else
        {
            text += "Contact: Not available \n";
        }
        text += "Order Details: \n";
        string jsonInput = payment["notes"]["line_items"].get<string>();
        string orderDetail = parseOrders(jsonInput);
        text += orderDetail;
        text += "Total Amt: Rs." + to_string(payment["amount"].get<int>() / 100) + " \n";

        // Add shipping address (line1 + line2)
        if (payment.contains("customer_details") && payment["customer_details"].contains("shipping_address"))
        {
            auto shippingAddress = payment["customer_details"]["shipping_address"];
            string address = shippingAddress["line1"].get<string>() + ", " + shippingAddress["line2"].get<string>();
            text += "Shipping Address: " + address + " \n";
        }
        else
        {
            text += "Shipping Address: Not available \n";
        }

        file << text; 

        file.close();

        cout << text;

        printOrder(id);
    }
    else
    {
        cerr << "Failed to write payment " << id << " to file.\n";
    }
}

void parseAndSavePayments(const string &response)
{
    try
    {
        json jsonResponse = json::parse(response);
        if (jsonResponse.contains("items"))
        {
            for (const auto &payment : jsonResponse["items"])
            {
                if (payment["status"].get<string>() == "paid")
                    savePaymentToFile(payment);
            }
        }
        else
        {
            cerr << "No 'items' found in the response.\n";
        }
    }
    catch (json::parse_error &e)
    {
        cerr << "JSON parsing error: " << e.what() << "\n";
    }
}

void pingAPI()
{
    CURL *curl;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl)
    {
        string readBuffer;

        // Create and set headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, AUTH_HEADER.c_str());
        headers = curl_slist_append(headers, "Accept: */*");
        headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate, br");
        headers = curl_slist_append(headers, "Connection: keep-alive");

        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Disable SSL verification

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
        }
        else
        {
            parseAndSavePayments(readBuffer);
        }

        // Free the headers and cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}

int main()
{
    const int INTERVAL = 30; // Set interval to 60 seconds

    while (true)
    {
        pingAPI();
        this_thread::sleep_for(chrono::seconds(INTERVAL));
    }

    return 0;
}
