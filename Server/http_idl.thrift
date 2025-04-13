namespace go esp32

struct Request {
    1: string message
}

struct Response {
    1: string message
}

service ExampleService {
    Response SayHello(1: Request req) (api.get="/hello")
}