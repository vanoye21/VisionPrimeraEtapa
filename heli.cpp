#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "SDL/SDL.h"
#include <stdlib.h>
#include "CHeli.h"
#include <unistd.h>
#include <stdio.h>
#include <iostream>

using namespace std;
using namespace cv;
Mat src; Mat hsv; Mat hue;
int bins = 25;

/// Function Headers
void Hist_and_Backproj(int, void* );
vector<Point> points;
vector<Mat> channelsHSV;
bool stop = false;
CRawImage *image;
int umbral, h, s ,v;
CHeli *heli;
float pitch, roll, yaw, height;
int hover;
// Joystick related
SDL_Joystick* m_joystick;
bool useJoystick;
int joypadRoll, joypadPitch, joypadVerticalSpeed, joypadYaw;
bool navigatedWithJoystick, joypadTakeOff, joypadLand, joypadHover;
int a=0,b=0, blue=0, green=0, red=0, hueTest=0, valueTest=0, saturationTest=0;
bool status = false;

void Hist_and_Backproj(int, void* )
{
  MatND hist;
  int histSize = MAX( bins, 2 );
  float hue_range[] = { 0, 180 };
  const float* ranges = { hue_range };

  /// Get the Histogram and normalize it
  calcHist( &hue, 1, 0, Mat(), hist, 1, &histSize, &ranges, true, false );
  normalize( hist, hist, 0, 255, NORM_MINMAX, -1, Mat() );

  /// Get Backprojection
  MatND backproj;
  calcBackProject( &hue, 1, 0, hist, backproj, &ranges, 1, true );

  /// Draw the backproj
  imshow( "BackProj", backproj );

  /// Draw the histogram
  int w = 400; int h = 400;
  int bin_w = cvRound( (double) w / histSize );
  Mat histImg = Mat::zeros( w, h, CV_8UC3 );

  for( int i = 0; i < bins; i ++ )
     { rectangle( histImg, Point( i*bin_w, h ), Point( (i+1)*bin_w, h - cvRound( hist.at<float>(i)*h/255.0 ) ), Scalar( 0, 0, 255 ), -1 ); }

  imshow( "Histogram", histImg );

}

void blackandwhite(const Mat &sourceImage, Mat &destinationImage)
{
	if (destinationImage.empty())
		destinationImage = Mat(sourceImage.rows, sourceImage.cols, sourceImage.type());

	int channels = sourceImage.channels();
	
	for (int y = 0; y < sourceImage.rows; ++y) 
	{
		uchar* sourceRowPointer = (uchar*) sourceImage.ptr<uchar>(y);
		uchar* destinationRowPointer = (uchar*) destinationImage.ptr<uchar>(y);

		for (int x = 0; x < sourceImage.cols ; ++x)
			{
				int redTemp = sourceRowPointer[x * channels +2];
				int greenTemp = sourceRowPointer[x * channels  +1];
				int blueTemp = sourceRowPointer[x * channels];
				destinationRowPointer[x] = (redTemp + greenTemp + blueTemp)/3;
			}
	}

}

void umbrales(const Mat &sourceImage, Mat &destinationImage, int umbral)
{
	if (destinationImage.empty())
		destinationImage = Mat(sourceImage.rows, sourceImage.cols, sourceImage.type());

	for (int y = 0; y < sourceImage.rows; ++y) 
	{
		uchar* sourceRowPointer = (uchar*) sourceImage.ptr<uchar>(y);
		uchar* destinationRowPointer = (uchar*) destinationImage.ptr<uchar>(y);

		for (int x = 0; x < sourceImage.cols ; ++x)
			{
				if (sourceRowPointer[x]>umbral)
				destinationRowPointer[x] = 255;
				else
			        destinationRowPointer[x] =0;
			}
	}
}

void obtenerRGB(const Mat &sourceImage)
{
	int channels = sourceImage.channels();
		uchar* sourceRowPointer2 = (uchar*) sourceImage.ptr<uchar>(b);
		if (status)
		{		
		red = sourceRowPointer2[(a ) * channels + 2];
		green = sourceRowPointer2[(a ) * channels + 1];
		blue = sourceRowPointer2[( a ) * channels + 0];
		}
}

void obtenerHSV(const Mat &sourceImage)
{
	int channels = sourceImage.channels();
		uchar* sourceRowPointer2 = (uchar*) sourceImage.ptr<uchar>(b);
		if (status)
		{		
		hueTest = sourceRowPointer2[(a ) * channels ];
		saturationTest = sourceRowPointer2[(a ) * channels + 1];
		valueTest = sourceRowPointer2[( a ) * channels + 2];
		}
}


void flipImageEfficient(const Mat &sourceImage, Mat &destinationImage)
{
	if (destinationImage.empty())
		destinationImage = Mat(sourceImage.rows, sourceImage.cols, sourceImage.type());

	int channels = sourceImage.channels();
	
	for (int y = 0; y < sourceImage.rows; ++y) 
	{
		uchar* sourceRowPointer = (uchar*) sourceImage.ptr<uchar>(y);
		uchar* destinationRowPointer = (uchar*) destinationImage.ptr<uchar>(y);
		for (int x = 0; x < sourceImage.cols / 2; ++x)
			for (int i = 0; i < channels; ++i)
			{
				destinationRowPointer[x * channels + i] = sourceRowPointer[(sourceImage.cols - 1 - x ) * channels + i];
				destinationRowPointer[(sourceImage.cols - 1 - x) * channels + i] = sourceRowPointer[x * channels + i];
			}
	}
}
void mouseCoordinatesExampleCallback(int event, int x, int y, int flags, void* param)
{
    switch (event)
    {
        case CV_EVENT_LBUTTONDOWN:
            cout << "Mouse X, Y: " << x << ", " << y ;
            cout << endl;
            /*  Draw a point */
            points.push_back(Point(x, y));
	    a = x;
            b = y;
	    status = true;
            //cout << a << "   " << b << endl;

            break;
        case CV_EVENT_MOUSEMOVE:
            break;
        case CV_EVENT_LBUTTONUP:
            break;
    }
}
// Convert CRawImage to Mat
void rawToMat( Mat &destImage, CRawImage* sourceImage)
{	
	uchar *pointerImage = destImage.ptr(0);
	
	for (int i = 0; i < 240*320; i++)
	{
		pointerImage[3*i] = sourceImage->data[3*i+2];
		pointerImage[3*i+1] = sourceImage->data[3*i+1];
		pointerImage[3*i+2] = sourceImage->data[3*i];
	}
}

int main(int argc,char* argv[])
{
	//establishing connection with the quadcopter
	heli = new CHeli();
	
	//this class holds the image from the drone	
	image = new CRawImage(320,240);

	//webcam
	//VideoCapture camera;
	//camera.open(0);
	
	// Initial values for control	
        pitch = roll = yaw = height = 0.0;
        joypadPitch = joypadRoll = joypadYaw = joypadVerticalSpeed = 0.0;

	// Destination OpenCV Mat	
	Mat currentImage = Mat(240, 320, CV_8UC3);
	Mat flippedImage = Mat(240, 320, CV_8UC3);
        Mat freezeImage = Mat(240, 320, CV_8UC3);
        Mat hsvImage = Mat(240, 320, CV_8UC3);
	Mat bwImage = Mat(240, 320, CV_8UC1);
	Mat umbralImage = Mat(240, 320, CV_8UC1);
	Mat colorHSV = Mat(240, 320, CV_8UC1);
	Mat colorRGB = Mat(240, 320, CV_8UC3);
    // Initialize joystick
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
    useJoystick = SDL_NumJoysticks() > 0;
    if (useJoystick)
    {
        SDL_JoystickClose(m_joystick);
        m_joystick = SDL_JoystickOpen(0);
    }

    while (stop == false)
    {

        // Clear the console
        //printf("\033[2J\033[1;1H");

        if (useJoystick)
        {
            SDL_Event event;
            SDL_PollEvent(&event);

            joypadRoll = SDL_JoystickGetAxis(m_joystick, 2);
            joypadPitch = SDL_JoystickGetAxis(m_joystick, 3);
            joypadVerticalSpeed = SDL_JoystickGetAxis(m_joystick, 1);
            joypadYaw = SDL_JoystickGetAxis(m_joystick, 0);
            joypadTakeOff = SDL_JoystickGetButton(m_joystick, 1);
            joypadLand = SDL_JoystickGetButton(m_joystick, 2);
            joypadHover = SDL_JoystickGetButton(m_joystick, 0);
        }

        // prints the drone telemetric data, helidata struct contains drone angles, speeds and battery status
        //printf("===================== Parrot Basic Example =====================\n\n");
        //fprintf(stdout, "Angles  : %.2lf %.2lf %.2lf \n", helidata.phi, helidata.psi, helidata.theta);
        //fprintf(stdout, "Speeds  : %.2lf %.2lf %.2lf \n", helidata.vx, helidata.vy, helidata.vz);
        //fprintf(stdout, "Battery : %.0lf \n", helidata.battery);
        //fprintf(stdout, "Hover   : %d \n", hover);
        //fprintf(stdout, "Joypad  : %d \n", useJoystick ? 1 : 0);
        //fprintf(stdout, "  Roll    : %d \n", joypadRoll);
        //fprintf(stdout, "  Pitch   : %d \n", joypadPitch);
        //fprintf(stdout, "  Yaw     : %d \n", joypadYaw);
        //fprintf(stdout, "  V.S.    : %d \n", joypadVerticalSpeed);
        //fprintf(stdout, "  TakeOff : %d \n", joypadTakeOff);
        //fprintf(stdout, "  Land    : %d \n", joypadLand);
        //fprintf(stdout, "Navigating with Joystick: %d \n", navigatedWithJoystick ? 1 : 0);
	
		//image is captured from parrot
		heli->renewImage(image);
		rawToMat(currentImage, image);
		//webcam image
		//camera >> currentImage;
		//flipImageEfficient(currentImage, flippedImage);
		obtenerRGB(currentImage);

		// Copy to OpenCV Mat from parrot


		//imshow("Flipped", flippedImage);

		namedWindow("Image");
		setMouseCallback("Image", mouseCoordinatesExampleCallback);
		
            //for (int i = 0; i < points.size(); ++i) {
           //     circle(currentImage, (Point)points[i], 10, Scalar( 0, 0, 255 ), CV_FILLED);
           // } 
		
		//if (i>=1)
		//line(currentImage, (Point)points[i-1], (Point)points[i], Scalar(0,0,255), 5, 8, 0);
		//}
		imshow("Image", currentImage);
		if (status)
		{
		cout << "R = " << red << " Green = " << green << "  Blue = " << blue << endl;
		freezeImage = currentImage;
		imshow("Freeze", freezeImage);
		//blackandwhite(freezeImage, bwImage);
		//imshow("BW", bwImage);
		//cout << "Umbrales?" << endl;
		//cin >> umbral;
		//umbrales(bwImage,umbralImage, umbral);
		//imshow("Umbral", umbralImage);
		cvtColor(freezeImage,hsvImage,CV_BGR2HSV);  
		imshow("HSV", hsvImage);
		obtenerHSV(hsvImage);
		cout << "Hue = " << hueTest << " Sat = " << saturationTest << "  Value = " << valueTest << endl;

 		inRange(hsvImage,Scalar(hueTest -20, saturationTest -40, valueTest -100),Scalar(hueTest+20, saturationTest+40,valueTest+80),colorHSV);

    		imshow("HSV Color", colorHSV); 
		//split(hsvImage, channelsHSV);
		//imshow("H", channelsHSV[0]);
		//imshow("S", channelsHSV[1]);
		//imshow("V", channelsHSV[2]);
		inRange(freezeImage,Scalar(red -20, green -20, blue -20),Scalar(red+20, green+20, blue+20),colorRGB);
    		imshow("RGB Color", colorRGB); 
		

  //hue.create( freezeImage.size(), freezeImage.depth() );
  //int ch[] = { 0, 0 };
  //mixChannels( &freezeImage, 1, &hue, 1, ch, 1 );

  /// Create Trackbar to enter the number of bins
  //const char* window_image = "Source image";
  //namedWindow( window_image, WINDOW_AUTOSIZE );
  //createTrackbar("* Hue  bins: ", window_image, &bins, 180, Hist_and_Backproj );
  //Hist_and_Backproj(0, 0);
 status = false;





		}
		
		




        char key = waitKey(5);
		switch (key) {
			case 'a': yaw = -20000.0; break;
			case 'd': yaw = 20000.0; break;
			case 'w': height = -20000.0; break;
			case 's': height = 20000.0; break;
			case 'q': heli->takeoff(); break;
			case 'e': heli->land(); break;
			case 'z': heli->switchCamera(0); break;
			case 'x': heli->switchCamera(1); break;
			case 'c': heli->switchCamera(2); break;
			case 'v': heli->switchCamera(3); break;
			case 'j': roll = -20000.0; break;
			case 'l': roll = 20000.0; break;
			case 'i': pitch = -20000.0; break;
			case 'k': pitch = 20000.0; break;
            case 'h': hover = (hover + 1) % 2; break;
            case 27: stop = true; break;
            default: pitch = roll = yaw = height = 0.0;
		}

        if (joypadTakeOff) {
            heli->takeoff();
        }
        if (joypadLand) {
            heli->land();
        }
        hover = joypadHover ? 1 : 0;

        //setting the drone angles
        if (joypadRoll != 0 || joypadPitch != 0 || joypadVerticalSpeed != 0 || joypadYaw != 0)
        {
            heli->setAngles(joypadPitch, joypadRoll, joypadYaw, joypadVerticalSpeed, hover);
            navigatedWithJoystick = true;
        }
        else
        {
            heli->setAngles(pitch, roll, yaw, height, hover);
            navigatedWithJoystick = false;
        }

        usleep(15000);
	}
	
	heli->land();
    SDL_JoystickClose(m_joystick);
    delete heli;
	delete image;
	return 0;
}

