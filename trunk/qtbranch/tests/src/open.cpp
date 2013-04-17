#include <iostream>
#include "opencv.hpp"
#include "opencvWebcam.h"

using namespace std;
using namespace cv;

int main( int argc, char **argv)
{
	int done = 0;
	int images = 0;
	opencvWebcam capture;

	capture.startCamera();

	if (!capture.isOpened() )
	{
		cout << "Can not open camera" << "\n";
		exit(1);
	}
	cvNamedWindow("w1");
	while (!done) {
		IplImage *image = capture.queryFrame();
		Mat img(image);
		
			cvShowImage("w1", image);
		if ( !(img.empty()) ) {
			images++;
		} else {
			switch cv::waitKey() {
				case 27:
                    done=1;
                    break;
                }
		}
	}
	capture.stopCamera();

	printf ("Images %d\n", images);

	return( images);
	
}
