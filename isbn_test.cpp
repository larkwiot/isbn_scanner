#include "src/util.hpp"
#include "ut/ut.hpp"

using namespace boost::ut;

int main() {
  auto console_log = spdlog::stdout_color_mt("console");
  auto error_log = spdlog::stdout_color_mt("stderr");
  spdlog::flush_every(std::chrono::seconds(5));
  spdlog::set_level(spdlog::level::debug);

  "Utility Functions"_test = [] {
    should("ctoi") = [] {
      expect(ctoi('0') == 0);
      expect(ctoi('1') == 1);
      expect(ctoi('2') == 2);
      expect(ctoi('3') == 3);
      expect(ctoi('4') == 4);
      expect(ctoi('5') == 5);
      expect(ctoi('6') == 6);
      expect(ctoi('7') == 7);
      expect(ctoi('8') == 8);
      expect(ctoi('9') == 9);
    };
  };

  "ISBN Validation"_test = [] {
    should("isbn10 valid") = [] {
      expect(is_valid_isbn("0071466932") == true);
      expect(is_valid_isbn("193176932X") == true);
      expect(is_valid_isbn("052159104X") == true);
      expect(is_valid_isbn("158113052X") == true);
      expect(is_valid_isbn("8425507006") == true);
      expect(is_valid_isbn("0534393217") == true);
    };

    should ("isbn10 invalid") = [] {
      expect(is_valid_isbn("1931769329") == false);
      expect(is_valid_isbn("1581130522") == false);
      expect(is_valid_isbn("8425507005") == false);
      expect(is_valid_isbn("053439XXXX") == false);
      expect(is_valid_isbn("12389X9814") == false);
      expect(is_valid_isbn("0000000000") == false);
      expect(is_valid_isbn("1111111111") == false);
    };

    should("isbn13 valid") = [] {
      expect(is_valid_isbn("9780735682931") == true);
      expect(is_valid_isbn("9780672328978") == true);
      expect(is_valid_isbn("9781447123309") == true);
      expect(is_valid_isbn("9780735682931") == true);
      expect(is_valid_isbn("9780735682931") == true);
      expect(is_valid_isbn("9781447123309") == true);
    };
    
    should("isbn13 invalid") = [] {
      expect(is_valid_isbn("978073568293X") == false);
      expect(is_valid_isbn("9780672328928") == false);
      expect(is_valid_isbn("9780735682932") == false);
      expect(is_valid_isbn("9780735482931") == false);
      expect(is_valid_isbn("9781447123308") == false);
    };
  };

  return 0;
}
