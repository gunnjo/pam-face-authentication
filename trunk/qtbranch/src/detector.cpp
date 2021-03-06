/*
    Detector Class - Inherits FACE DETECTOR AND EYE DETECTOR CLASS
    Copyright (C) 2010 Rohan Anil (rohan.anil@gmail.com) -BITS Pilani Goa Campus
    http://www.pam-face-authentication.org

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <tracker.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/imgproc/imgproc_c.h>
#include "detector.h"
#include "utils.h"
#include "qtUtils.h"

#define eyeSidePad 30
#define eyeTopPad 30
#define eyeBottomPad 120

using std::bad_alloc;

//------------------------------------------------------------------------------
detector::detector() : messageIndex(FACE_NOT_ACQUIRED), boolClipFace_(0), 
  prevlengthEye_(0), inAngle_(0),
  lengthEye_(0), widthEyeWindow_(0), heightEyeWindow_(0), 
  finishedClipFaceFlag_(0)
{
}

//------------------------------------------------------------------------------
detector::~detector()
{
    while ( !clippedFaces.empty() ) {
        cvReleaseImage( &clippedFaces.back());
        clippedFaces.pop_back();
    }
}

//------------------------------------------------------------------------------
IplImage* preprocess(IplImage* img, CvPoint plefteye, CvPoint prighteye)
{
  double xvalue = prighteye.x - plefteye.x;    
  double yvalue = prighteye.y - plefteye.y;
  double ang = atan(yvalue / xvalue) * (180 / CV_PI);
  double width = sqrt(pow(xvalue,2) + pow(yvalue, 2));
  double ratio = sqrt(pow(xvalue, 2) + pow(yvalue, 2))/80;
  double sidePad = eyeSidePad * ratio;
  double topPad = eyeTopPad * ratio;
  double bottomPad = eyeBottomPad * ratio;
  CvPoint p1LeftTop, p1RightBottom;
    
  IplImage* face = cvCreateImage(cvSize(140, 150), 8, 3);
  IplImage* imgDest = cvCreateImage(cvSize(img->width, img->height), 8, 3);
  cvZero(face);
  
  p1LeftTop.x = plefteye.x - sidePad;
  p1LeftTop.y = plefteye.y - topPad;
  p1RightBottom.x = plefteye.x + width + sidePad;
  p1RightBottom.y=plefteye.y + bottomPad;
  rotate(ang, plefteye.x, plefteye.y, img, imgDest);
  cvSetImageROI(imgDest, 
    cvRect(p1LeftTop.x, p1LeftTop.y, 
      p1RightBottom.x - p1LeftTop.x, 
      p1RightBottom.y - p1LeftTop.y));
  cvResize(imgDest, face, CV_INTER_LINEAR);
  cvResetImageROI(imgDest);
    
  cvReleaseImage( &imgDest);
  return face;
}

//------------------------------------------------------------------------------
IplImage* detector::clipFace(IplImage* inputImage)
{
  if(inputImage == 0) return 0;

  if(eyesInformation.LE.x > 0 && eyesInformation.LE.y > 0 &&
     eyesInformation.RE.x > 0 && eyesInformation.RE.y > 0)
  {
    IplImage* face = preprocess(inputImage, eyesInformation.LE, eyesInformation.RE);
    return face;
  }
  else return 0;

}

//------------------------------------------------------------------------------
void detector::updateEyeTracker( tracker &eye, CvPoint &centerPoint, IplImage *gray)
{
    IplImage* grayIm1 = cvCreateImage(cvSize(gray->width/2, gray->height), 8, 1);
    cvSetImageROI(gray, cvRect(0, 0, (gray->width)/2, gray->height) );
    cvResize(gray, grayIm1, CV_INTER_LINEAR);
    cvResetImageROI(gray);
    eye.setModel(grayIm1);
    eye.anchorPoint = centerPoint;
    cvReleaseImage(&grayIm1);

}
//------------------------------------------------------------------------------
int detector::runDetector(IplImage* input)
{
  static int flag;
  double xvalue, yvalue, currentAngle;
  double angle, angle2, v1, v2, v3, lengthTemp;
  int newWidth,newHeight;
  int lxdiff = 0, rxdiff = 0, lydiff = 0, rydiff = 0;
  int newWidthR,newHeightR,newWidthL,newHeightL;
  CvPoint temp, leftEyePTemp, leftEyePointRelativeTemp;
  CvPoint rightEyePTemp, rightEyePointRelativeTemp;

  messageIndex = FACE_NOT_ACQUIRED;
  if(input == 0) return -1;
  CvSize imageSize = cvGetSize(input);
  int minFaceWidth = imageSize.width * .3;
  int maxFaceWidth = imageSize.width * .9;
  int minFaceHeight = imageSize.height * .4;
  int maxFaceHeight = imageSize.height * .9;

  // Runs the face detector onto the input Face
  runFaceDetector(input);

  if(checkFaceDetected() == true)
  {
    if(faceInformation.Width < minFaceWidth || faceInformation.Height < minFaceHeight)
      messageIndex = FACE_TOO_SMALL; // Come closer to the camera
    else if(faceInformation.Width > maxFaceWidth || faceInformation.Height > maxFaceHeight)
      messageIndex = FACE_TOO_LARGE; // Go farer away from the camera
    else
    {
      IplImage* clipFaceImage = clipDetectedFace(input);
      
      // With the clipped face, run the eye detector
      runEyesDetector(clipFaceImage, input, faceInformation.LT);
      
      if(checkEyeDetected() == true)
      {
        flag = 1;
        
        IplImage *gray = cvCreateImage(cvSize(clipFaceImage->width,
          clipFaceImage->height / 2), 8, 1);
        cvSetImageROI(clipFaceImage, 
          cvRect(0, (clipFaceImage->height)/8, clipFaceImage->width,
            (clipFaceImage->height)/2) );
        cvCvtColor(clipFaceImage, gray, CV_BGR2GRAY);
        cvResetImageROI(clipFaceImage);

        leftEyeP_.x = eyesInformation.LE.x;
        leftEyeP_.y = eyesInformation.LE.y;
        rightEyeP_.x = eyesInformation.RE.x;
        rightEyeP_.y = eyesInformation.RE.y;
        
        xvalue = rightEyeP_.x-leftEyeP_.x;
        yvalue = rightEyeP_.y-leftEyeP_.y;
        
        inAngle_ = atan(yvalue / xvalue);
        leftEyePointRelative_.x = eyesInformation.LE.x - faceInformation.LT.x;
        leftEyePointRelative_.y = eyesInformation.LE.y - faceInformation.LT.y
          - (clipFaceImage->height)/8;
        rightEyePointRelative_.x = eyesInformation.RE.x - faceInformation.LT.x 
          - (gray->width)/2;
        rightEyePointRelative_.y = eyesInformation.RE.y - faceInformation.LT.y
          - (clipFaceImage->height)/8;

        lengthEye_ = eyesInformation.Length;
        prevlengthEye_ = eyesInformation.Length;
        widthEyeWindow_ = gray->width/2;
        heightEyeWindow_ = gray->height;

        updateEyeTracker( leftEye_, leftEyePointRelative_, gray);
        updateEyeTracker( rightEye_, rightEyePointRelative_, gray);

        cvReleaseImage(&gray);
      }
      else
      {
        messageIndex = FACE_NOT_TRACKING; // Face, but no Eyes.
      }
      
      cvReleaseImage(&clipFaceImage);
    }
 
  }
  else
  {
    if(flag != 1) messageIndex = FACE_NOT_FOUND; // No face detected.
  }

  if(flag == 1)
  {
    newWidth = floor( (prevlengthEye_ * widthEyeWindow_) / lengthEye_);
    newHeight =  floor( (prevlengthEye_ * heightEyeWindow_) / lengthEye_);
  }

  if(flag == 1 && newWidth > 0 && newHeight > 0 && prevlengthEye_ > 0)
  {
    CvMat* rotateMatrix = cvCreateMat(2, 3, CV_32FC1);
    xvalue = rightEyeP_.x - leftEyeP_.x;
    yvalue = rightEyeP_.y - leftEyeP_.y;
    currentAngle = atan(yvalue / xvalue) * (180 / CV_PI);
    currentAngle -= inAngle_;

    CvPoint2D32f centre;
    centre.x = leftEyeP_.x;
    centre.y = leftEyeP_.y;
    cv2DRotationMatrix(centre, currentAngle, 1.0, rotateMatrix);
        
    IplImage* dstimg = cvCreateImage(cvSize(input->width, input->height), 
      8, input->nChannels);
    cvWarpAffine(input, dstimg, rotateMatrix, CV_WARP_FILL_OUTLIERS, cvScalarAll(0));
    CvPoint rotatedRightP;
    rotatedRightP.x = floor( rightEyeP_.x * CV_MAT_ELEM(*rotateMatrix, float, 0, 0) 
                          +  rightEyeP_.y * CV_MAT_ELEM(*rotateMatrix, float, 0, 1) 
                          + CV_MAT_ELEM(*rotateMatrix, float, 0, 2));
    rotatedRightP.y = floor( rightEyeP_.x * CV_MAT_ELEM(*rotateMatrix, float, 1, 0) 
                          +  rightEyeP_.y * CV_MAT_ELEM(*rotateMatrix, float, 1, 1) 
                          + CV_MAT_ELEM(*rotateMatrix, float, 1, 2));
    rightEyeP_.x = rotatedRightP.x;
    rightEyeP_.y = rotatedRightP.y;

    newWidthR = newWidth;
    newHeightR = newHeight;
    newWidthL = newWidth;
    newHeightL = newHeight;

    int ly = leftEyeP_.y - int(floor(((leftEyePointRelative_.y) * newWidth) / widthEyeWindow_));
    int lx = leftEyeP_.x - int(floor(((leftEyePointRelative_.x) * newWidth) / widthEyeWindow_));

    if(lx < 0)
    {
      lxdiff = -lx;
      lx = 0;
    }
    
    if(ly < 0)
    {
       lydiff = -ly;
       ly = 0;
    }
    
    if( (lx + newWidth) > input->width) newWidthL = input->width - lx;
    if( (ly +newHeight) > input->height) newHeightL = input->height - ly;

    IplImage* grayIm1 = cvCreateImage(cvSize(newWidthL, newHeightL), 8, 1);
    cvSetImageROI(dstimg, cvRect(lx, ly, newWidthL, newHeightL) );
    cvCvtColor(dstimg, grayIm1, CV_BGR2GRAY);
    cvResetImageROI(dstimg);
    
    int rx = rightEyeP_.x - int(floor(((rightEyePointRelative_.x) * newWidth) / widthEyeWindow_));
    int ry = rightEyeP_.y - int(floor(((rightEyePointRelative_.y) * newWidth) / widthEyeWindow_));

    if(ry < 0)
    {
      rydiff = -ry;
      ry = 0;
    }
    
    if (rx < 0)
    {
      rxdiff = -rx;
      rx = 0;
    }
    
    if( (rx+newWidth) > input->width) newWidthR = input->width - rx;
    if( (ry+newHeight) > input->height) newHeightR = input->height - ry;

    IplImage *grayIm2 = cvCreateImage(cvSize(newWidthR, newHeightR), 8, 1);
    cvSetImageROI(dstimg,cvRect(rx, ry, newWidthR, newHeightR));
    cvCvtColor(dstimg, grayIm2, CV_BGR2GRAY);
    cvResetImageROI(dstimg);

    leftEye_.trackImage(grayIm1);
    rightEye_.trackImage(grayIm2);

    leftEye_.findPoint(leftEyePointRelative_, &temp);
    leftEyePTemp.y = leftEyeP_.y -(((leftEyePointRelative_.y) * newWidth) / widthEyeWindow_) + lydiff + temp.y;
    leftEyePTemp.x = leftEyeP_.x -(((leftEyePointRelative_.x) * newWidth) / widthEyeWindow_) + lxdiff + temp.x;
    leftEyePointRelativeTemp.x = temp.x;
    leftEyePointRelativeTemp.y = temp.y;
   
    rightEye_.findPoint(rightEyePointRelative_, &temp);
    rightEyePTemp.y = rightEyeP_.y - (((rightEyePointRelative_.y) * newWidth) / widthEyeWindow_) + rydiff + temp.y;
    rightEyePTemp.x=rightEyeP_.x - (((rightEyePointRelative_.x) * newWidth) / widthEyeWindow_) + rxdiff + temp.x;
    rightEyePointRelativeTemp.x = temp.x;
    rightEyePointRelativeTemp.y = temp.y;
    
    angle = atan(double(rightEyePTemp.y - leftEyePTemp.y) / double(rightEyePTemp.x - leftEyePTemp.x)) * 180 / CV_PI;
    angle2 = atan(double(rightEyeP_.y - leftEyeP_.y) / double(rightEyeP_.x-leftEyeP_.x)) * 180 / CV_PI;
    v1 = sqrt(pow(rightEyePTemp.y - rightEyeP_.y, 2) + pow(rightEyePTemp.x -rightEyeP_.x, 2));
    v2 = sqrt(pow(leftEyePTemp.y - leftEyeP_.y, 2) + pow(leftEyePTemp.x - leftEyeP_.x, 2));
    v3 = v1 + v2;
    lengthTemp = sqrt(pow(rightEyePTemp.y - leftEyePTemp.y, 2) 
                    + pow(rightEyePTemp.x - leftEyePTemp.x, 2));

    if(pow( (angle2 - angle), 2) < 300 && v1 < 140 && v2 < 144 && lengthTemp > 1)
    {
      messageIndex = FACE_FOUND;
      leftEyeP_ = leftEyePTemp;
      rightEyeP_  = rightEyePTemp;
      cv2DRotationMatrix(centre, -currentAngle, 1.0, rotateMatrix);
      CvPoint antiRotateR, antiRotateL;
      antiRotateR.x = floor( rightEyeP_.x * CV_MAT_ELEM(*rotateMatrix, float, 0, 0) 
                          +  rightEyeP_.y * CV_MAT_ELEM(*rotateMatrix, float, 0, 1) 
                          + CV_MAT_ELEM(*rotateMatrix, float, 0, 2));
      antiRotateR.y = floor( rightEyeP_.x * CV_MAT_ELEM(*rotateMatrix, float, 1, 0) 
                          +  rightEyeP_.y * CV_MAT_ELEM(*rotateMatrix, float, 1, 1) 
                          + CV_MAT_ELEM(*rotateMatrix, float, 1, 2));
      antiRotateL.x = floor( leftEyeP_.x * CV_MAT_ELEM(*rotateMatrix, float, 0, 0) 
                          +  leftEyeP_.y * CV_MAT_ELEM(*rotateMatrix, float, 0, 1) 
                          + CV_MAT_ELEM(*rotateMatrix, float, 0, 2));
      antiRotateL.y = floor( leftEyeP_.x * CV_MAT_ELEM(*rotateMatrix, float, 1, 0) 
                          +  leftEyeP_.y * CV_MAT_ELEM(*rotateMatrix, float, 1, 1) 
                          + CV_MAT_ELEM(*rotateMatrix, float, 1, 2));
      
      leftEyeP_ = antiRotateL;
      rightEyeP_ = antiRotateR;

      eyesInformation.LE.x = leftEyeP_.x;
      eyesInformation.LE.y = leftEyeP_.y;
      eyesInformation.RE.x = rightEyeP_.x;
      eyesInformation.RE.y = rightEyeP_.y;

      eyesInformation.Length = sqrt(pow(eyesInformation.RE.y - eyesInformation.LE.y, 2)
        + pow(eyesInformation.RE.x - eyesInformation.LE.x, 2));

      prevlengthEye_ = eyesInformation.Length;
#ifdef PFA_IMAGE_MANGLE
      cvLine(input, eyesInformation.LE, eyesInformation.RE, cvScalar(0,255,0), 4);
#endif
    }
    
    cvReleaseImage(&dstimg);
    cvReleaseImage(&grayIm1);
    cvReleaseImage(&grayIm2);
    cvReleaseMat(&rotateMatrix);
  }

  if(checkEyeDetected() == true && checkFaceDetected() == true && boolClipFace_ == true)
  {
      clippedFaces.push_back(clipFace(input));
      messageIndex = FACE_CAPTURED;
  }

  return 0;
}

//------------------------------------------------------------------------------
int detector::getClipFaceCounter()
{
  return clippedFaces.size();
}

//------------------------------------------------------------------------------
IplImage** detector::returnClipedFace()
{
    IplImage** temp = new IplImage* [clippedFaces.size()];
    for ( int i = 0; i < clippedFaces.size(); i++ ) {
        temp[i] = clippedFaces[i];

    }

    return temp;
}

//------------------------------------------------------------------------------
void detector::startClipFace()
{
    //Clear any oldones... they should be ignored
    while ( !clippedFaces.empty() ) {
        cvReleaseImage( &clippedFaces.back());
        clippedFaces.pop_back();
    }
  
  boolClipFace_ = true;
}

//------------------------------------------------------------------------------
void detector::stopClipFace()
{
  boolClipFace_ = false;
  
}

//------------------------------------------------------------------------------
int detector::detectorSuccessful()
{
  return (messageIndex == FACE_FOUND);
}

//------------------------------------------------------------------------------
int detector::queryMessage()
{
  return messageIndex;
}
// vim: set ts:2 sw:2 et ai

