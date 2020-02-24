// Copyright 2018 Your Name <your_email>
#include <header.hpp>
#include <gumbo.h>

using boost::program_options::options_description;
using boost::program_options::variables_map;
using boost::program_options::error;
using boost::program_options::store;
using boost::program_options::notify;
using boost::program_options::value;

void parseRecursively(void* output, std::ofstream& outputFile){
    auto node = *reinterpret_cast<GumboNode*>(output);
    if (node.type == GUMBO_NODE_ELEMENT) {
        if (node.v.element.tag == GUMBO_TAG_IMG) {
            auto length = node.v.element.attributes.length;
            for (auto i = 0u; i < length; ++i) {
                if (std::strcmp(
                        (*reinterpret_cast<
                        GumboAttribute*
                        >(node
                        .v
                        .element
                        .attributes
                        .data[i])).name,
                        "src") != 0){
                    outputFile << (
                    (*reinterpret_cast<
                    GumboAttribute*
                    >(node
                    .v
                    .element
                    .attributes
                    .data[i]))).value << '\n';
                    break;
                }
            }
        }
        auto length = node.v.element.children.length;
        for (auto i = 0u; i < length; ++i) {
            parseRecursively(node.v.element.children.data[i], outputFile);
        }
    }
}
int main(int argc, char* argv[]){
    try {
        options_description desc{"Options"};
        desc.add_options()
                ("url",
                        value<std::string>()->required(),
                        "url")
                ("depth",
                        value<int>()->required(),
                        "depth")
                ("network_threads",
                        value<int>()->default_value(1),
                        "network thread count")
                ("parser_threads",
                        value<int>()->default_value(1),
                        "parser thread count")
                ("output",
                        value<std::string>()->required(),
                        "output file path");
        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);
        std::ofstream outputFile(vm["output"].as<std::string>());
        io_context ioContext;
        tcp::resolver resolver(ioContext);
        tcp_stream stream(ioContext);
        auto url = URL(vm["url"].as<std::string>());
        auto const results = resolver.resolve(url.host, url.port);
        stream.connect(results);
        request<string_body> req{verb::get, url.path, 10};
        req.set(field::host, url.host);
        req.set(field::user_agent, BOOST_BEAST_VERSION_STRING);
        write(stream, req);
        flat_buffer buffer;
        response<dynamic_body> res;
        read(stream, buffer, res);
        std::ostringstream os;
        os << boost::beast::make_printable(buffer.data());
        std::string s = os.str();
        GumboOutput* output = gumbo_parse(s.c_str());
        auto childrenCount = output->root->v.element.children.length;
        for (auto i = 0u; i < childrenCount; ++i) {
            parseRecursively(
                    output->root->v.element.children.data[i],
                    outputFile);
        }
        gumbo_destroy_output(new GumboOptions, output);
        boost::beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::beast::errc::not_connected) {
            throw boost::beast::system_error(ec);
        }
        outputFile.close();
    } catch (const error& ex){
        std::cerr << ex.what() << std::endl;
    }
}
