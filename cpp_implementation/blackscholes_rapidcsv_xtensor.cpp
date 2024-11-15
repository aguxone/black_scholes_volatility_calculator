#include <limits>
#include <fstream> // For opening file
#include <xtensor/xio.hpp> // Inrclude this header for inprinting
#include <xtensor/xcsv.hpp> // For exporting to csv at the end
#include <xtensor/xview.hpp>
#include "rapidcsv.h"
#include "blackscholes_rapidcsv_xtensor_functions.h"
// NOTA DE COMPILACION:
// Compila con exactamente C++17, ni más ni menos por tendremos errores de todo tipo.
// VERIFICAR POR QUE TIRA CEROS AHORA

using namespace std;
using namespace rapidcsv;


// Nota: Diferencias con notebook:
// Se checkea que el nombre del archivo a procesar sea correcto
// Comentarios acerca del estado del procesamiento en cout

int main(){

    cout << "Starting data processing";
    // LabelParams (a,b), defecto es (0,-1), pero dejarlo así. Con a=0 usa la primer fila de header, los interpreta como string.
    // El b se me hace misterioso dejarlo en -1, pero si le ponemos 0, interpreta TODO como string dps.
    // ConverterParams(true) ES PARA MISSING VALUES (y Wrong): Para datos float les pone el tipo std::numeric_limits<long double>::signaling_NaN(), y para ints es 0 (pero como long long, no int).
    // FACILMENTE PODEMOS DECIDIR NOSOTROS el tipo llamando la funcion así ConverterParams(true, ELTIPOPARAFLOAT, ELTIPOPARAINT, true), yo RECOMIENDO usar NaN de tipo DOUBLE si en nuestro código usamos double para los float, y tipo int para int si es nuestro código usa "ints" (y no long long que es feo). SI DEJAMOS LOS DEFAULT VAMOS A TENER WARNING DE UN "NARROWING" porque acá definimos un longdouble de nan y en el código se usa un double y a mí ME CAGABA por ejemplo los nanamean interpretaba MAL los NAN!, OJO!
    // Ejemplo si los queiro float e ints como NaN pongo ConverterParams (false,std::numeric_limits<long double>::signaling_NaN(),std::numeric_limits<long double>::signaling_NaN(), true)
    // NOTA: Para un int no se considera NaN como tan válido no sé por qué, se usa quizá std::numeric_limits<long double>::max() que es el máximo int representable que es gigantezco.
    // NOTA 2: ES NECESARIO ADEMÁS para realizar conversiones en general depsués estos parámetros así que está bueno tenerlos definidos de UNA
    // NOTA 3: Si queremos los default NO los ponemos, o escribimos rapidcsv:ConverterParams(true) (que es poner el primer parámetro nomás)
    const string filePath = "./Exp_Octubre.csv";
    ifstream input_file(filePath);
    if (!input_file) {
      cerr << "Error: Could not find the file at " << filePath
           << ". Please check the file path and try again." << endl;
      return EXIT_FAILURE;
    }
    rapidcsv::Document doc;
    ConverterParams params(true, numeric_limits<long double>::signaling_NaN(), numeric_limits<long long>::signaling_NaN(), true);
    try {
    // const static Document doc(filePath, LabelParams(), SeparatorParams(';'), ConverterParams(params) );
    doc = rapidcsv::Document(filePath, LabelParams(), SeparatorParams(';'), ConverterParams(params));
    // Dps de este comando guardó los datos como byte digamos, después los podemos castear a lo que queramos y allí habrá una intepreteación de cómo eran los datos
    // de manera individual o en vectors que es lo más útil.
    // Lo bueno es que lo podemos castear a lo que queramos básicamente.
    } 
    catch (const std::exception& e) {
        cerr << "An error occurred while reading the file: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    std::cout << "\nStarting csv pre-processing";

    // Removemos las columnas que no usaremos en absoluto
    doc.RemoveColumn("description");
    doc.RemoveColumn("kind");
    
    // Extract the columns as strings
    // En los nombres de las variables nos alcanza con dejarlo como auto.
    // PERO, en GetColumn podemos CASTEAR (entre brackets), interpretó un tipo pero si era int nos los pasa a float, o lo dejamos como int is queremos
    // EPA está bueno

    // Creamos un vector que contenga todas las columnas (casteadas como string, las vamos a modificar)

    vector<vector<string>> numeric_columns_str =
    {
        doc.GetColumn<string>("bid"), // COL 0 
        doc.GetColumn<string>("ask"), // COL 1
        doc.GetColumn<string>("underBid"), // COL 2
        doc.GetColumn<string>("underAsk"), // COL 3
    };

    // Guardamos la columna de fechas de manera separada
    auto created_at = doc.GetColumn<string>("created_at");
    // Convertimos a segundos con la función ya definida para ser manipulado adecuadamente con xtensor
    // auto created_at_seconds = convert_dates_to_seconds(created_at);

    // Hay que realizar las siguientes modificaciones
    // - reemplazar comas por puntos praa correcta interpretacion
    // - convertir de strin a double teniendo en cuenta valroes incorrectos como nan

    // CREAMOS una nueva lista-vector donde colocaremos las filas con las modificaciones necesarias
    vector< vector<double> > numeric_columns;

    // Realizamos las modificaciones a las columnas anteriores y las agregamos a este vector final

    for (const auto& column : numeric_columns_str)
    {
        // Creamos una columna temporal a modificar
        vector<double> modified_column;

        // Modificamos dentro de los items de cada columna
        for (auto value : column)
        {
            // Reemplazamos las comas por puntos
            replace(value.begin(), value.end(), ',', '.'); // Replace commas with points
            double tempdouble;
            // Convertimos de string a double, usamos rapidcsv mismo que maneja datos incorrectos automaticamente
            // (o sea rapidcsv::Converter)
            // (alternativa con un if sería std::stof)
            Converter<double>(params).ToVal(value, tempdouble );
            //
            modified_column.push_back( tempdouble );
        }
        // Colocamos la columna modificada en la lista-vector de columnas finales
        numeric_columns.push_back(modified_column);
    }

    // CALCULOS
    // TENEMOS QUE CALCULAR
    // CHECK calcular promedios de opciones y stocks (avopt y avstock) momento a momento
    // - desvíos estándar históricos (entre bid y price de opciones más fácil)
    // - desvío estándar "by minute", hacer un shift primero de los datos o ver
    // - time to maturities a todo momento
    // - predicción de call por blackscholes

    // Algunos parametros para pretty printing de xtensor
    xt::print_options::set_line_width(200);  // Sets max line width for each row
    xt::print_options::set_precision(2);    // Sets precision for floating-point numbers
    xt::print_options::set_threshold(10);    // Sets threshold for number of elements to print before truncating

    std::cout << "\nStarting calculations";

    // Convertimos a segundos con la función ya definida para ser manipulado adecuadamente con xtensor
    auto created_at_seconds = convert_dates_to_seconds(created_at);

    // Creamos un xarray para ser manipulado, adaptando el vector 2d de columans numericas que
    // obtuvimos con rapidcsv
    xt::xarray<double> numeric_columns_xarray = adapt_vector_2d(numeric_columns);
    // Utilizamos unsigned long long en vez de int porque así es el tipo y no casteamso ni tenemos warnings ni nada.
    unsigned long long numberofrows = numeric_columns_xarray.shape()[0];

    // Calculamos los average de opciones (avopt) y de acciones(avstock), y calculamos el std historical instantaneo (de cada fila como está)
    xt::xarray<double> avopt = xt::nanmean ( xt::view(numeric_columns_xarray, xt::all(),xt::range(0, 2)) , {1});
    avopt.reshape({numberofrows,1}); // COL 4
    xt::xarray<double> avstock = xt::nanmean ( xt::view(numeric_columns_xarray, xt::all(),xt::range(2, 4)) , {1});
    avstock.reshape({numberofrows,1}); // COL 5
    // Ojo el std dev, {1} es el eje, y el 1 dps es "sample standard deviation" (n-1) en vez de la population
    xt::xarray<double> std_instant_historical = xt::stddev( xt::view( numeric_columns_xarray, xt::all(), xt::range(2,4) ) , {1}, 1) ;
    std_instant_historical.reshape({numberofrows,1}); // COL 6
    // PROBLEMA EN WINDOWS, sobre-escribir la variable resulta en datos erróneos al menos en este paso. Por eso lo escirbimos
    // en una sección nueva de la memoria y posteriormente pisamos la variable anterior.
    xt::xarray<double> temp_array1  = xt::concatenate(xt::xtuple(numeric_columns_xarray, avopt,avstock,std_instant_historical), 1);
    numeric_columns_xarray = std::move(temp_array1);

    // Para el std_by_minute (un minuto menos el anterior) hacemos una columna nueva corrida un lugar hacia abajo o sea un SHIFT,
    // al cual vamos a hacer un stacking con una fila vacía
    // y además seleccionamos HASTA la anteúltima fila, para eso contamos filas totales y restamos uno.
    xt::xarray<double>empty_row {std::nan("")};
    empty_row.reshape({1,1});
    xt::xarray<double> under_Bid_prev = xt::concatenate(  xt::xtuple (  empty_row , xt::view( numeric_columns_xarray,  xt::range(0,numberofrows-1) , xt::keep(2) ) ),  0 ); // COL 7
    xt::xarray<double> under_Ask_prev = xt::concatenate(  xt::xtuple (  empty_row , xt::view( numeric_columns_xarray,  xt::range(0, numberofrows-1 ), xt::keep(3) ) ),  0 ); // COL 8

    // SEGUIMOS PREPARANDO PARA LOS STD BY MINUTE
    // Concatenaos todas las columnas prmero
    xt::xarray<double> temp_array2 = xt::concatenate(xt::xtuple(numeric_columns_xarray, under_Bid_prev, under_Ask_prev), 1);
    numeric_columns_xarray = std::move(temp_array2);
    // Los average de los previous (los shifteados)
    xt::xarray<double> avstockprev = xt::nanmean( xt::view (numeric_columns_xarray, xt::all(), xt::range(7,9) ), {1} ); // COL 9
    avstockprev.reshape({numberofrows, 1}); 
    // Reconcatenamos esta última fila con los average price de stock shifteados
    xt::xarray<double> temp_array3 = xt::concatenate(xt::xtuple(numeric_columns_xarray, avstockprev), 1); 
    numeric_columns_xarray = std::move(temp_array3);

    // // Calculamos std by minute de una vez ahora entre los average de underbidask y uniderbidaskprev
    // // que son avstock y avstockprev
    // Creo un xarray temporal para esto porque no le gusta calcular cosas entre columnas NO contiguas lamentablemente
    xt::xarray<double> avstockavprev = xt::view( numeric_columns_xarray, xt::all(), xt::keep(5,9) );
    xt::xarray<double> std_by_minute = xt::stddev ( avstockavprev, {1}, 1); // COL 10
    std_by_minute.reshape({numberofrows,1});
    // Concatenamos al original
    xt::xarray<double> temp_array4 = xt::concatenate(xt::xtuple(numeric_columns_xarray, std_by_minute), 1); 
    numeric_columns_xarray = std::move(temp_array4);

    // PASAMOS A LOS CALCULOS CON FECHAS
    // Convertimos segundos created at a un xarray
    xt::xarray<double> created_at_seconds_xarray = xt::adapt( created_at_seconds, {3296,1} );// COL 11

    // Apendeamos estos segundos
    xt::xarray<double> temp_array5 = xt::concatenate(xt::xtuple(numeric_columns_xarray, created_at_seconds_xarray), 1); 
    numeric_columns_xarray = std::move(temp_array5);
    // Escribimos la fecha de vencimiento de opciones como vector 1D y la transformamos
    std::vector<std::string> vencimiento = {{"10/20/2023 18:00"}};
    auto vencimiento_seconds_str = convert_dates_to_seconds(vencimiento);
    xt::xarray<double> vencimiento_seconds = xt::adapt (vencimiento_seconds_str, {1,1});
    // Le restmoas entonces el tiempo a cada fila para obtener time to maturity
    xt::xarray<double> timetomaturity_seconds =  vencimiento_seconds - created_at_seconds_xarray;
    // Pasamos el tiempo a años!
    xt::xarray<double> timetomaturity_years = timetomaturity_seconds / 31104000; // COL 12
    // Agregamos el timetomaturity_years al xarray
    xt::xarray<double> temp_array6 = xt::concatenate(xt::xtuple(numeric_columns_xarray, timetomaturity_years), 1); 
    numeric_columns_xarray = std::move(temp_array6);

    // Comenzamos a calcular lo importante las volatilidades
    // Definimos los parámetros de black-scholes para la predicción
    const double lower_bound = 0.001;
    const double upper_bound = 2.0;
    double K = 1033.0;  // Strike price, default 100
    double r = 0.9;   // Risk-free interest rate
    double tol = 1e-6;
    int max_iter = 1000;

    // Array vacío con zeros para guardar el resultado
    xt::xarray<double> bisection_results = xt::zeros<double>({numberofrows}); // COL 13;
    for ( unsigned long long i = 0; i < numberofrows; ++i) {
        double S = numeric_columns_xarray(i, 5);  // avstock (column 5)
        double price = numeric_columns_xarray(i, 4); // avopt (column 4)
        double T = numeric_columns_xarray(i, 12); // time_to_maturity (column 12)
        bisection_results(i) = implied_volatility_bisection(S, K, r, T, price, lower_bound, upper_bound, tol, max_iter)[0];    
    };
    // Reshapeamos el resultado
    bisection_results.reshape({numberofrows,1});
    // Concantenamos
    xt::xarray<double> temp_array7 = xt::concatenate(xt::xtuple(numeric_columns_xarray, bisection_results), 1); 
    numeric_columns_xarray = std::move(temp_array7);
    // numeric_columns_xarray  = xt::concatenate(xt::xtuple(numeric_columns_xarray, bisection_results), 1); // COL 13

    // // Extraemos sólo las columnas deseadas
    auto selected_columns = xt::view(numeric_columns_xarray, xt::all(), xt::keep(6, 10, 13));
    // selected_columns;

    std::cout << "\nEnded processing data";
    std::cout << "\nWriting to file";

    // // Exportamos a un csv
    // std::ofstream file2("cpp_full.csv");
    // xt::dump_csv(file2, numeric_columns_xarray);
    std::ofstream file("cpp_selected_columns_exp_octubre.csv");
    xt::dump_csv(file, selected_columns);
    input_file.close();

    cout << "\nProcessing ended";

    return 0;
}