
        #include <functional>
        #include <numeric>
        #include <vector>
        int main()
        {
        std::vector<float> a(5);
        std::vector<float> b(5);
        auto c = std::transform_reduce(cbegin(a), cend(a), cbegin(b), 0, std::plus<>{}, std::multiplies<>{}); };
