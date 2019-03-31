
#include <iostream>
#include "rpc.h"
#include "json.h"

/*
void test_json2019() {
    Buffer buf;
    //buf.add("{\"jsonrpc\": \"2.0\", \"method\": \"/domain/add\", \"params\": [42, 23], \"id\": 456}");
    buf.add("{\"jsonrpc\": \"2.0\", \"method\": \"/rpc/add\", \"id\": 456, \"params\": {\"name\": \"/domain/add\"}}");
    //buf.add("{\"json       ");

    std::cout << buf.as_str() << std::endl;
    
    JsonParser r;
    try {
        if(r.parse_object(buf.slice()) == 1) cout << "-- skipped body\n";
        std::cout << "Request id: " << r.id.as_string() << ", method: " << r.method.as_string() << ", params: " << r.params.as_string(1) << std::endl;
        if(r.method.equal("/rpc/add") && r.params.valid()) {
            JsonParser params;
            params.parse_object(r.params);

            std::cout << "Name: " << params.name.as_string() << std::endl;
        }
    } catch (Exception &e) {
        std::cout << "Exception: " << e.code << ": " << e.get_msg() << std::endl;
    }
}
*/

int main(int argc, char** argv) {
    std::cout << "Start server!\n";
    
    RpcServer server;
    try {
        server.start(8001);
    } catch (const char * str) {
        std::cout << "Exception: " << str << std::endl;
    }

    return 0;
}
