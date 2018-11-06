#include <string>
#include <vector>
#include <algorithm>

namespace threadsafe
{

std::string generate_random_string()
{
    int size = rand() % 1000 + 1;

    std::string str;
    str.reserve(size);

    std::generate_n(std::back_inserter(str), size, rand);

    return str;
}

std::vector<std::string> generate_random_strings(int strings_count)
{
    std::vector<std::string> strings_pushed;
    strings_pushed.reserve(strings_count);

    std::generate_n(std::back_inserter(strings_pushed), strings_count, generate_random_string);

    return strings_pushed;
}

} // namespace threadsafe
