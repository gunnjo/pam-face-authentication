/* TRANSLATOR std::faceVerifyer */

/*
    QT Face Verifyer MAIN
    Copyright (C) 2013 Joseph Gunn (joe@josephgunn.com)

	Based on work originally:
    Copyright (C) 2013 Rohan Anil (rohan.anil@gmail.com) -BITS Pilani Goa Campus
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

#include <QtGui>
#include <QApplication>

#include <iostream>
#include <list>
#include <string>
#include <cctype>
#include <dirent.h>
#include <unistd.h>

#include "faceVerifier.h"
#include "aboutBox.h"
#include "webcamImagePaint.h"
#include "utils.h"
#include "cv.h"
#include "highgui.h"
#include "qtUtils.h"
#include "pam_face_defines.h"

#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
#include <pwd.h>

//------------------------------------------------------------------------------
faceVerifyer::faceVerifyer(QWidget* parent) :
    QMainWindow(parent),
    imageReturned(0),
    faceFound(0),
    faceVerified(0),
    imageSave(false),
    timerId(0)
{
    ui.setupUi(this);
    QDesktopWidget* desktop = QApplication::desktop();

    int screenWidth = desktop->width(); // get width of screen
    int screenHeight = desktop->height(); // get height of screen

    QSize windowSize = size(); // size of our application window
    int width = windowSize.width();
    int height = windowSize.height();

    // little computations
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    y -= 50;

    // move window to desired coordinates
    move(x, y);

    ui.stkWg->setCurrentIndex(0);
    connect(ui.pb_next_t1, SIGNAL(clicked()), this, SLOT(showTab2()));
    connect(ui.pb_next_t2, SIGNAL(clicked()), this, SLOT(showTab3()));
    connect(ui.pb_about, SIGNAL(clicked()), this, SLOT(about()));

}

//------------------------------------------------------------------------------
faceVerifyer::~faceVerifyer()
{
}


//------------------------------------------------------------------------------
void faceVerifyer::showTab1()
{
    killTimer(timerId);
    webcam.stopCamera();

    ui.stkWg->setCurrentIndex(0);
}

//------------------------------------------------------------------------------
void faceVerifyer::showTab3()
{
    killTimer(timerId);
    ui.stkWg->setCurrentIndex(2);
	webcam.stopCamera();
}

//------------------------------------------------------------------------------
void faceVerifyer::showTab2()
{
    connect(ui.pb_capture, SIGNAL(clicked()), this, SLOT(captureClick()));

    if(webcam.startCamera() == true)
    {
        ui.stkWg->setCurrentIndex(1);
        populateQList();
        timerId = startTimer(20);
    }
    else
    {
        QMessageBox msgBox1;
        msgBox1.setStyleSheet(QString::fromUtf8("background-color: rgb(255, 255, 255);"));
        msgBox1.setWindowTitle(tr("Face Verifyer"));
        msgBox1.setText(tr("<strong>Error</strong>"));
        msgBox1.setInformativeText(tr(
                                       "Camera Not Found. <br /> "
                                       "Plugin Your Camera and Try Again."));
        msgBox1.setStandardButtons(QMessageBox::Ok);
        msgBox1.setIconPixmap(QPixmap(":/data/ui/images/cnc.png"));
        msgBox1.exec();
    }
}

//------------------------------------------------------------------------------
void faceVerifyer::populateQList()
{
    ui.lv_thumbnails->clear();
    setFace* faceSetStruct = newVerifier.getFaceSet();

    for(int i = 0; i < faceSetStruct->count; i++)
    {
        char setName[100];
        snprintf(setName, 100,  "Set %d", i+1);
        // ui.lv_thumbnails->setIconSize(QSize(60, 60));
        
        QListWidgetItem *item = new QListWidgetItem(setName, ui.lv_thumbnails);
        item->setIcon(QIcon(faceSetStruct->setFilePathThumbnails[i]));
        QString qstring(faceSetStruct->setName[i]);
        // cout << faceSetStruct->setName[i] << endl;
        item->setData(Qt::UserRole, qstring);
    }
	newVerifier.releaseFaceSet( faceSetStruct);
}

//------------------------------------------------------------------------------
void faceVerifyer::setQImageWebcam(QImage* input)
{
    if(!input) return;

    static QGraphicsScene* scene = new QGraphicsScene(0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
    ui.webcamPreview->setScene(scene);
    ui.webcamPreview->setBackgroundBrush(*input);

    ui.webcamPreview->show();

}

//------------------------------------------------------------------------------
aboutBox::aboutBox(QWidget* parent) : QDialog(parent)
{
    ui.setupUi(this);
}

//------------------------------------------------------------------------------
void faceVerifyer::about()
{
    aboutBox newAboutBox;
    newAboutBox.exec();
}

//------------------------------------------------------------------------------
void faceVerifyer::removeSelected()
{
    QList<QListWidgetItem*> list = ui.lv_thumbnails->selectedItems();
    QList<QListWidgetItem*>::iterator i;

    for(i = list.begin(); i != list.end(); ++i)
    {
        QListWidgetItem* item = *i;
        QString dat = item->data(Qt::UserRole).toString();
        

        newVerifier.removeFaceSet( dat.toAscii().data());
    }
    ui.lv_thumbnails->clear();

    populateQList();
}

//------------------------------------------------------------------------------
void faceVerifyer::captureClick()
{
    
    if(!imageSave)
    {
        ui.pb_capture->setText(tr("Cancel Saving"));
        imageSave = true;
        newDetector.startClipFace();
    }
    else
    {
        ui.pb_capture->setText(tr("Capture Failures"));
        imageSave = false;
        newDetector.stopClipFace();
    }
}

//------------------------------------------------------------------------------
QString faceVerifyer::getQString(int messageIndex)
{
    /* sprintf(messageCaptureMessage, 
        "Captured %d/%d faces.",
        totalFaceClipNum - clipFaceCounter + 1,
        totalFaceClipNum);*/

    switch (messageIndex ) {
        case FACE_TOO_SMALL:
            return QString(tr("Please come closer to the camera."));
        case FACE_TOO_LARGE:
            return QString(tr("Please go little far from the camera."));
        case FACE_NOT_FOUND:
            return QString(tr("Unable to Detect Your Face."));
        case FACE_NOT_TRACKING:
            return QString(tr("Tracker lost, trying to reinitialize."));
        case FACE_FOUND:
            return QString(tr("Tracking in progress."));
        case FACE_CAPTURED:
            return QString(tr("Captured %1 / %2 faces.").arg(FACE_COUNT-newDetector.getClipFaceCounter()+1).arg(FACE_COUNT));
        case FACE_CAPTURE_FINISHED:
            return QString(tr("Capturing Image Finished."));
        case FACE_NOT_ACQUIRED:
        default:
            return QString("");
       }

    return 0;
}

void faceVerifyer::saveImage( cv::Mat image) {
	static uid_t userID = getuid();
	static passwd* userStruct = getpwuid(getuid());
	static std::string dir (userStruct->pw_dir);
	dir.append("/" PFA_PATH "/diags/");
	const char *prefix = "fvsav";
	int r = mkdir(dir.c_str(), 0700);
	if ( !r || errno == EEXIST ) {
		char * name = tempnam(dir.c_str(), prefix);
		if ( name) {
			char fullname[300];
			snprintf( fullname, 300, "%s.jpg", name); 
			imwrite( fullname, image);
			free(name);
		}
	}

}

//------------------------------------------------------------------------------
void faceVerifyer::setIbarText(QString message)
{
    ui.lbl_ibar->setText(message);
}

//------------------------------------------------------------------------------
void faceVerifyer::timerEvent(QTimerEvent*)
{
    try {
        static webcamImagePaint newWebcamImagePaint;
        cv::Mat img;
    
        webcam >> img;
        IplImage queryImage(img);

        if (newDetector.runDetector(&queryImage) == -1)
            return;
        imageReturned++;

	int verified = 0;
        if (newDetector.checkFaceDetected() > 0 ) {
            faceFound++;
            IplImage *clippedFace = newDetector.clipFace(&queryImage);
            verified = newVerifier.verifyFace(clippedFace);
            if (verified > 0 ) faceVerified++;
            cvReleaseImage(&clippedFace);
        }
#ifdef PFA_GEN_STATS
            printf("Verified %d out of %d with %d faces this one was (%s)\n", faceVerified, imageReturned, faceFound, verified > 0 ? "Found" : "NotFound");
#endif
        QString qs = " ";
	if ( faceFound) { // Anytime after the first face was found
		if (verified > 0) {
			qs = QString(tr("    Verified %1 / %2 faces in %3 images.").arg(faceVerified).arg(faceFound).arg(imageReturned, 3, 10, QChar(' ')));
		} else {
			if ( imageSave) {
				saveImage(img);
			}
			qs = QString(tr("Not Verified %1 / %2 faces in %3 images.").arg(faceVerified).arg(faceFound).arg(imageReturned, 3, 10, QChar(' ')));
		}
	} else { // Otherwise give a helpfull setup message
		switch (verified) {
			case -1:
				qs = QString(tr(" Not Trained %1 / %2 faces in %3 images.").arg(faceVerified).arg(faceFound).arg(imageReturned, 3, 10, QChar(' ')));
				break;
			case -2:
				qs = QString(tr(" No Data dir %1 / %2 faces in %3 images.").arg(faceVerified).arg(faceFound).arg(imageReturned, 3, 10, QChar(' ')));
				break;
			case 0:
			default:
				qs = getQString(newDetector.queryMessage());
				break;
		}
	}
        setIbarText( qs);
        newWebcamImagePaint.paintCyclops(&queryImage, 
                newDetector.eyesInformation.LE, newDetector.eyesInformation.RE);
        newWebcamImagePaint.paintEllipse(&queryImage, 
                newDetector.eyesInformation.LE, newDetector.eyesInformation.RE);

        QImage* qm = QImageIplImageCvt(&queryImage);

        setQImageWebcam(qm);

        cvWaitKey(1);

        delete qm;
    } catch ( std::exception &e) {
        qFatal( "Standard error exception (%s)",
                e.what());
    } catch ( ... ) {
        qFatal( "Unknown exception");
    }
    return;
}

bool faceVerifyer::notify(QObject *object, QEvent *event)
{
    return false;
}
// vim: set ts:4 sw:4 et ai
