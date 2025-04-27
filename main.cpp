#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <sys/random.h>

#include <gmpxx.h>


constexpr double BITS_PER_WORD = 5 * std::log2(6.0);         // ≈12.926 bits
static constexpr char CHARSET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "!@#$%^&*()-_=+[]{};:,.<>?/|";
constexpr std::size_t C = sizeof(CHARSET) - 1;              // 94


// For illustrative purposes, a normal password generator
std::string random_equivalent(std::size_t dice_words) {
    double total_bits = dice_words * BITS_PER_WORD;
    std::size_t length = static_cast<std::size_t>(
        std::ceil(total_bits / std::log2(static_cast<double>(C)))
    );

    std::string out;
    out.reserve(length);

    for (std::size_t i = 0; i < length; ++i) {
        uint8_t byte;
        do {
            ssize_t r = getrandom(&byte, 1, GRND_RANDOM);
            if (r < 0) throw std::runtime_error("getrandom failed");
        } while (byte >= uint8_t(256 - (256 % C)));

        out += CHARSET[byte % C];
    }
    return out;
}


constexpr uint32_t WORDLIST_SIZE = 7776;
constexpr std::array<uint16_t, 5> POWERS_OF_6 = {1296, 216, 36, 6, 1};
constexpr uint8_t THROWS_REQUIRED = 5;

uint8_t random6() {
    uint8_t res;
    char buf[1];
    ssize_t ret = getrandom(buf, 1, GRND_RANDOM);
    if (ret < 0) throw std::runtime_error("getrandom failed: " + std::string(std::strerror(errno)));
    if (ret != 1) return 255;
    res = static_cast<uint8_t>(buf[0]);
    if (res < 252) return res % 6; // We would loose entropy otherwise
    return 255;
}

uint8_t throw_dice() {
    uint8_t res = 255;
    while (res == 255) {
        res = random6();
    }
    return res;
}

std::size_t get_word_pos() {
    std::size_t result = 0;
    for (uint8_t i = 0; i < THROWS_REQUIRED; i++) {
        uint16_t throw_res = throw_dice();
        result += throw_res * POWERS_OF_6[i];
    }
    return result;
}

std::vector<std::string> load_wordlist() {
    std::string line;
    std::vector<std::string> words;
    std::ifstream wordlist("./wordlist.txt");

    if (!wordlist.is_open()) {
        throw std::runtime_error("Couldn't open ./wordlist.txt");
    }
    while (std::getline(wordlist, line)) {
        words.emplace_back(line);
    }
    if (words.size() != WORDLIST_SIZE) {
        std::cout << "WARNING! Unexpected wordlist size: " << words.size() << '\n';
        std::cout << "Your password might turn out to be weaker than calculated." << std::endl;
    }
    return words;
}

std::string diceware(std::size_t len) {
    if (len == 0) return "";
    std::string password;
    std::vector<std::string> wordlist = load_wordlist();

    for (std::size_t i = 0; i + 1 < len; i++) {
        password += wordlist[get_word_pos()] + '.';
    }
    password += wordlist[get_word_pos()];
    return password;
}

mpz_class calculate_strength(std::size_t len) {
    mpz_class base("7776");
    uint64_t exponent = len;
    mpz_class result;
    mpz_pow_ui(result.get_mpz_t(), base.get_mpz_t(), exponent);
    return result;
}

int do_main_work(int argc, char* argv[]) {
    if (argc <= 1) {
        std::cout << "Usage: " << argv[0] << " <words_count>" << std::endl;
        return EXIT_SUCCESS;
    }
    std::size_t len = std::stoull(argv[1]);

    std::string password = diceware(len);
    mpz_class strength = calculate_strength(len);
    std::cout << "New password: " << password << '\n';
    std::cout << "This password would need around " << strength
              << " tries needed to break, which is roughly equivalent to 2^"
              << mpz_sizeinbase(strength.get_mpz_t(), 2) - 1 << ".\n";
    std::cout << "A typical password with this entropy would look like this: \n" << random_equivalent(len) << std::endl;

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    try {
        return do_main_work(argc, argv);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
