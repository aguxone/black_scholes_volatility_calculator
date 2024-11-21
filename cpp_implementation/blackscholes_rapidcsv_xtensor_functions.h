// blackscholes_rapidcsv_xtensor_functions.h
#pragma once

#include <ctime> // los tipos time_t y funcion mktime para crear fechas
#include <iomanip> // para usar get_time y parsear fechas
#include <iostream> // para tener cout si es necesario
#include <cmath> //para raíz cuadrada
#include <string>
#include <vector>
#include <xtensor/xarray.hpp> // para adaptar vector 2d lo coloco aquí
#include <xtensor/xadapt.hpp>


std::vector<std::time_t> convert_dates_to_seconds(const std::vector<std::string>& date_strings) 
{
    std::vector<std::time_t> seconds_since_epoch;

    for (const auto& date_str : date_strings) {
        std::tm tm = {};
        std::istringstream ss(date_str);

        // Parse the date string using the appropriate format
        ss >> std::get_time(&tm, "%m/%d/%Y %H:%M");
        if (ss.fail()) {
            std::cerr << "Failed to parse date: " << date_str << std::endl;
            continue; // Skip this entry on failure
        }

        // Convert to time_t (seconds since epoch)
        std::time_t time = std::mktime(&tm);
        seconds_since_epoch.push_back(time);
    }

        return seconds_since_epoch;
};

const double sqrt_0_5 = sqrt(0.5);

// Cumulative distribution function as function of native complementary error function in c++
double normalCDF(double z_value) {
    return 0.5 * erfc( - (z_value * sqrt_0_5) );
    // return 0.5 * (1 + erf(z_value * sqrt_0_5) ) ;
    // return 0.5 * erfc( (z_value / sqrt_0_5) );
}

// Define the function to optimize (Black-Scholes formula)
double function_to_optimize(double S, double K, double r, double T, double price, double sigma) {
    // Multiplying instead of using pow is generally faster
    double d1 = (log(S / K) + ( (r + (0.5 * sigma * sigma) ) * T ) ) / (sigma * sqrt(T));
    double d2 = d1 - ( sigma * sqrt(T) );
    double n1 = normalCDF(d1);
    double n2 = normalCDF(d2);
    double discount_factor = exp(-r * T);
    double call_price = (S * n1 ) - (K * discount_factor * n2);

    // std::cout << "d1: " << d1 << "\n";
    // std::cout << "d2: " << d2 << "\n";
    // std::cout << "n1: " << n1 << "\n";
    // std::cout << "n2: " << n2 << "\n";

    return call_price - price; // Function should be zero at the root, we are going to use Newton - Rhapsod iterative method
}

// Function to find implied volatility using the bisection method
std::vector<double> implied_volatility_bisection(double S, double K, double r, double T, double price, double lower_bound, double upper_bound, double tol = 1e-6, int max_iter = 1000) {
    // Define initial bounds for implied volatility
    // const double lower_bound = 0.001;
    // const double upper_bound = 2.0;
    double sigma_low = lower_bound;
    double sigma_high = upper_bound;
        

    // Ensure that observed price lies between calculated prices at initial bounds
    // We check for the "sign", the correct value is zero 0, so for bolzano one boundarie must be negative and
    // the other one positive, giving the multipication a negative number
    if (function_to_optimize(S, K, r, T, price, sigma_low) * function_to_optimize(S, K, r, T, price, sigma_high) > 0) {
        // std::cout << "OUT OF BOUNDS";
        return {NAN, NAN, NAN}; // Return NaN if bounds are incorrect
    }
    

    std::vector<double> result;
    // Bisection loop
    for (int iter = 0; iter < max_iter; ++iter) {
        double sigma_mid = (sigma_low + sigma_high) / 2;
        double call_price_mid = function_to_optimize(S, K, r, T, price, sigma_mid);
        // std::printf("The call price mid is  %.2f\n", call_price_mid);
        // std::cout << "The price mid is" << call_price_mid;

        // This call_price_mid here should ideally be equal to zero
        if (std::abs(call_price_mid) < tol) {
            result.push_back(sigma_mid);
            result.push_back(call_price_mid);
            result.push_back(iter);
            return result;
        }

        if (call_price_mid < 0) {
            sigma_low = sigma_mid;
        } else {
            sigma_high = sigma_mid;
        }
    }

    // std::cout << "No convergence";
    return {NAN, NAN, NAN}; // Return NaN if no convergence within max iterations
}

// Function to adapt a 2D std::vector to an xt::xarray
// Funcion, de output un xarray. De parámetros input un vector de entrada
xt::xarray<double> adapt_vector_2d(const std::vector<std::vector<double>>& vec) {
    // Get the shape of the 2D vector
    std::vector<size_t> shape = {vec.size(), vec.empty() ? 0 : vec[0].size()};

    // Flatten the 2D vector into a 1D vector
    std::vector<double> flat_vec;
    for (const auto& row : vec) {
        // Simplemente insertamos cada vector de a uno en el vector nuevo
        flat_vec.insert(flat_vec.end(), row.begin(), row.end());
    }

    // Adapt the flattened vector as an xarray
    // We must traspose the final matrix to show correctly
    return xt::transpose( xt::adapt(flat_vec, shape) );
};
