#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/photo.hpp"
#include <iostream>
#include <math.h>
#include <vector>


using namespace cv;
using namespace std;


Mat src_gray;
Mat src_bw;
Mat detected_edges;
Mat denoise;

int thresh = 30;

vector<vector<Point> > contours;

//VARIABLES PARA APLICAR REGRESIÓN LINEAR, ACTUALMENTE NO SE UTILIZAN
vector<double> ErrSumValues;  // Vector que guarda el error acumulado para cada uno de los contornos en Regresión Lineal
vector<double> ErrMValues; // Error que guarda el error de la variable M para cada contorno en Regresión Lineal
vector<double> ErrNValues; // Error que guarda el error de la variable N para cada contorno en Regresión Lineal


vector<vector<Point>> Distances; // Vector que, para cada contorno, guarda las distancias en los ejes de ordenadas y abscisas entre los puntos a tener en cuenta
vector<vector<double> > Relaciones; // Vector que, para cada contorno,  guarda la correlación (división) entre pares de distancias (ambos ejes)
int actualContour;

double N = 0;
double M = 0;

int fibras = 0;
int microPlasticos = 0;

RNG rng(12345);

void SetContoursVector(int, void* );
void ShowContoursCords();
void ClasifyContours();
void CleanContours();
void CalculateDataLR(); //FUNCIÓN QUE APLICA REGRESIÓN LINEAL, NO SE UTILIZA
void getDistances();


int main( int argc, char** argv )
{

    const char* source_window = "Source";
    CommandLineParser parser( argc, argv, "{@input | imagenes_seleccionadas/img2.jpg | input image}" );
    Mat src = imread( samples::findFile( parser.get<String>( "@input" ) ));
    if( src.empty() )
    {
      cout << "Could not open or find the image!\n" << endl;
      cout << "Usage: " << argv[0] << " <Input image>" << endl;
      return -1;
    }

    blur( src, src, Size(3,3));
    cvtColor( src, src_gray, COLOR_BGR2GRAY );
    
    //src_gray.convertTo(contrast, -1, 1.5, 0);

    namedWindow( source_window );
    imshow( source_window, src_gray );

    src_bw = src_gray > 164;
    //src_bw = src_gray > 200;
    
    //createTrackbar( "Canny thresh:", source_window, &thresh, max_thresh, thresh_callback );

    actualContour = 0;

    SetContoursVector( 0, 0 );
    ShowContoursCords();

    ClasifyContours();

    int regular = 0;

    for(int i = 0; i < Relaciones.size() ; i++)
    {
        for(int j = 0; j < Relaciones[i].size() ; j++)
        {

            if(Relaciones[i][j] <= 1.45 && Relaciones[i][j] >= 0.45)
                regular++;
        }

        if(regular >= 4)
            microPlasticos++;
        else
            fibras++;
        regular = 0;
    }

    //ErrSumValues.clear();
    Distances.clear();
    Relaciones.clear();

    cout << "En esta imagen hay " << fibras << " fibras y " << microPlasticos << " microplásticos." << endl; 

    waitKey();
    
    return 0;
}


void SetContoursVector(int, void* )
{
    Mat canny_output;
    vector<Vec4i> hierarchy;

    Canny( src_bw, canny_output, thresh, thresh*1.5 );
    imshow( "Canny", canny_output );

    findContours( src_bw, contours, hierarchy, RETR_LIST, CHAIN_APPROX_NONE );
    Mat drawing = Mat::zeros( src_bw.size(), CV_8UC3 );

    if(contours.size() > 0)
        CleanContours();

    for( int i = 0; i< contours.size(); i++ )
    {  
            Scalar color = Scalar( rng.uniform(0, 256), rng.uniform(0,256), rng.uniform(0,256) );
            drawContours( drawing, contours, (int)i, color, 2, LINE_8, hierarchy, 0 );
    }

    cout << endl << "Hay " << contours.size() << " contornos" << endl << endl;
    imshow( "Contours", drawing );

}

void ShowContoursCords()
{
    for( size_t i = 0; i< contours.size(); i++ )
    {
        //if(i == actualContour)
        //{
            cout << "CONTORNO " << i  << ": "<<  " con " << contours[i].size() << " puntos." << endl;
            for( size_t j = 0; j< contours[i].size(); j++ )
            {
                cout << j << ":  " << contours[i][j]  << endl;
            }

            cout << endl;
        //}
        
    }
}


void ClasifyContours()
{
    //cout << "He entrado en ClasifyContours" << endl << endl;
    double aux;
    vector<double> auxRel;

    for(int i = 0; i < contours.size(); i++)
    {   

            actualContour = i;
            cout << "CONTORNO " << i << " con " << contours[i].size() << " puntos." << endl;


            getDistances();
            

            for(int j = 0; j < Distances[i].size(); j++)
            {
                cout << "Distancia " << j << ": X-> " << Distances[i][j].x << " Y-> " << Distances[i][j].y << endl;
                aux = ((float)Distances[i][j].y)/((float)Distances[i][j].x);

                if(aux < 0)
                    aux *= -1;

                cout << "Relación " << j << "-> " << aux << endl;
                auxRel.push_back(aux);

            }

            cout << endl;
            Relaciones.push_back(auxRel);  
            auxRel.clear(); 
            
    }


    
}

void CalculateDataLR()
{
    //cout << "He entrado en CalculateData" << endl << endl;

    double sumX = 0;
    double sumX2 = 0;
    double sumY = 0;
    double sumXY = 0;
    double aux = 0;
    double errSum = 0;
    double errM = 0;
    double errN = 0;

    int tam = contours[actualContour].size();


    N = 0;
    M = 0;

    for(int i = 0; i < tam; i++)
    {
        sumX += contours[actualContour][i].x;
        sumX2 += contours[actualContour][i].x * contours[actualContour][i].x;
        sumY += contours[actualContour][i].y;
        sumXY += contours[actualContour][i].x + contours[actualContour][i].y;
    }

    //cout << "Valores calculados: " << endl << "  - SumX -> " << sumX << endl << "  - SumX2 -> " << sumX2 << endl << "  - SumY -> " << sumY << endl << "  - SumXY -> " << sumXY << endl; 

    M = ((tam*sumXY) - (sumX*sumY))/((tam*sumX2) - (sumX*sumX));
    N = ((sumX2*sumY) - (sumX*sumY))/((tam*sumX2) - (sumX*sumX));
    //N = (sumY - B * sumX)/tam;
    N = (sumY - sumX)/tam;

    for(int i = 0; i < tam; i++)
    {

        aux = N + (M*contours[actualContour][i].x) - contours[actualContour][i].y;
        aux *= aux;
        errSum  += aux;

    }

    errN = (sumX2/(tam*sumX2 - sumX*sumX)) * (errSum/(tam-2));
    errM = (tam/(tam*sumX2 - sumX*sumX)) * (errSum/(tam-2));


    errN = sqrt(errN);
    errM = sqrt(errM);


    ErrSumValues.push_back(errSum);
    ErrMValues.push_back(errM);
    ErrNValues.push_back(errN);




}

void getDistances()
{
    vector<Point> auxDistance;
    Point aux;

    int tam = contours[actualContour].size() - 1;
    

    for( float i = 0; i <= 0.875; i += 0.125 )
    {
        aux.x = contours[actualContour][tam * (i + 0.125)].x - contours[actualContour][tam * i].x;
        aux.y = contours[actualContour][tam * (i + 0.125)].y - contours[actualContour][tam * i].y;
        auxDistance.push_back(aux);
    }


    Distances.push_back(auxDistance);
    auxDistance.clear();

    

}


void CleanContours()
{
    int maxTam = 0;
    for( int i = 0; i < contours.size(); i++ )
    {
            if( contours[i].size() < 50)
            {
                if( contours[i].size() > 16)
                    microPlasticos++;

                contours.erase(contours.begin() + i);
                i--;

            }
            else if(contours[i].size() > contours[maxTam].size())
                maxTam = i;


    }

    contours.erase(contours.begin() + maxTam);
    
}

