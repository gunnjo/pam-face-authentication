/* TRANSLATOR std::faceTrainer */

/*
    QT Face Trainer MAIN
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

#include <QtGui>
#include <QApplication>

#include <iostream>
#include <list>
#include <string>
#include <cctype>
#include <dirent.h>
#include <unistd.h>

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "faceTrainer.h"
#include "faceTrainerAdvSettings.h"
#include "aboutBox.h"
#include "webcamImagePaint.h"
#include "utils.h"
#include "qtUtils.h"
#include "pam_face_defines.h"

using namespace cv;

//------------------------------------------------------------------------------
faceTrainer::faceTrainer(QWidget* parent) :
	QMainWindow(parent), timerId(0), learning(false)
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
    connect(ui.pb_capture, SIGNAL(clicked()), this, SLOT(captureClick()));
    connect(ui.pb_next_t2, SIGNAL(clicked()), this, SLOT(showTab3()));
    connect(ui.pb_next_t1, SIGNAL(clicked()), this, SLOT(showTab2()));
    connect(ui.pb_back_t2, SIGNAL(clicked()), this, SLOT(showTab1()));
    connect(ui.pb_about, SIGNAL(clicked()), this, SLOT(about()));
    connect(ui.pb_ds, SIGNAL(clicked()), this, SLOT(removeSelected()));
    connect(ui.pb_adv, SIGNAL(clicked()), this, SLOT(showAdvDialog()));

    // connect(ui.button1, SIGNAL(clicked()), this, SLOT(verify()));
    // connect(ui.but, SIGNAL(clicked()), this, SLOT(butClick()));
}

//------------------------------------------------------------------------------
faceTrainer::~faceTrainer()
{
}

//------------------------------------------------------------------------------
void faceTrainer::showTab1()
{
    killTimer(timerId);
    webcam.stopCamera();

    ui.stkWg->setCurrentIndex(0);
}

//------------------------------------------------------------------------------
void faceTrainer::showTab2()
{
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
        msgBox1.setWindowTitle(tr("Face Trainer"));
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
void faceTrainer::showTab3()
{
    killTimer(timerId);
    ui.stkWg->setCurrentIndex(2);
    webcam.stopCamera();
}

//------------------------------------------------------------------------------
void faceTrainer::populateQList()
{
    ui.lv_thumbnails->clear();
    setFace* faceSetStruct = newVerifier.getFaceSet();

    for(int i = 0; i < faceSetStruct->count; i++)
    {
        char setName[100];
        snprintf(setName, 100, "Set %d", i+1);
        // ui.lv_thumbnails->setIconSize(QSize(60, 60));
        
        QListWidgetItem *item = new QListWidgetItem(setName, ui.lv_thumbnails);
        item->setIcon(QIcon(faceSetStruct->setFilePathThumbnails[i]));
        QString qstring(faceSetStruct->setName[i]);
        // cout << faceSetStruct->setName[i] << endl;
        item->setData(Qt::UserRole, qstring);
    }
}

//------------------------------------------------------------------------------
void faceTrainer::setQImageWebcam(QImage* input)
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
void faceTrainer::about()
{
    aboutBox newAboutBox;
    newAboutBox.exec();
}

//------------------------------------------------------------------------------
void faceTrainer::showAdvDialog()
{
    faceTrainerAdvSettings newDialog( webcam, this,
        newVerifier.configDirectory);
    newDialog.initConfig();
    newDialog.sT(&newDetector, &newVerifier);
    newDialog.exec();
}

//------------------------------------------------------------------------------
/// Currently unused!
void faceTrainer::verify()
{
    struct dirent* de = NULL;

    DIR* d = opendir("/home/notroot/train/");
    while((de = readdir(d)))
    {
        if( !((strcmp(de->d_name, ".") == 0) 
           || (strcmp(de->d_name, "..") == 0)) )
        {
            char fullPath[300];
            snprintf(fullPath, 300, "%s/%s","/home/notroot/train",de->d_name);
            IplImage* temp = cvLoadImage(fullPath, 1);
            printf("%s\n", fullPath);
            if(newVerifier.verifyFace(temp) > 0) printf("%s\n", fullPath);
        }
    }

}

//------------------------------------------------------------------------------
void faceTrainer::removeSelected()
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
void faceTrainer::captureClick()
{
    
    if(!learning)
    {
        ui.pb_capture->setText(tr("Cancel"));
        learning = true;
        newDetector.startClipFace();
    }
    else
    {
        ui.pb_capture->setText(tr("Capture"));
        learning = false;
        newDetector.stopClipFace();
    }
}

//------------------------------------------------------------------------------
QString faceTrainer::getQString(int messageIndex)
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
            return QString(tr("Captured %1 / %2 faces.").arg(newDetector.getClipFaceCounter()).arg(FACE_COUNT));
        case FACE_CAPTURE_FINISHED:
            return QString(tr("Capturing Image Finished."));
        case FACE_NOT_ACQUIRED:
        default:
            return QString("");
	}

    return 0;
}

//------------------------------------------------------------------------------
void faceTrainer::setIbarText(QString message)
{
    ui.lbl_ibar->setText(message);
}

//------------------------------------------------------------------------------
void faceTrainer::timerEvent(QTimerEvent*)
{
	try {
		static webcamImagePaint newWebcamImagePaint;
		Mat img;
		int detected;

		webcam >> img;
		IplImage queryImage(img);

		detected = newDetector.runDetector(&queryImage);

		setIbarText(getQString(newDetector.queryMessage()));

		if (detected == -1 )
			return; // Try again another day


		if(learning && newDetector.getClipFaceCounter() > FACE_COUNT)
		{
static string setname = "";
			setIbarText(tr("Processing Faces, Please Wait ..."));
			// cvWaitKey(1000);
			if ( setname.length() == 0) {
				setname = newVerifier.addFaceSet(newDetector.returnClipedFace(), newDetector.getClipFaceCounter());
				populateQList();
			} else {
				if((newDetector.getClipFaceCounter() % 6) == 0) {
					newVerifier.addFaceSet(newDetector.returnClipedFace(), newDetector.getClipFaceCounter(), setname );
				}
				if( newVerifier.verifyFace(newDetector.clipFace(&queryImage)) == true) {
					captureClick();
					IplImage **imgs = newDetector.returnClipedFace();
					int count = newDetector.getClipFaceCounter();
					newVerifier.addFaceSet(imgs, count, setname );
					setIbarText(tr("Processing Completed."));
					setname = "";
				}
			}

		}

		newWebcamImagePaint.paintCyclops(&queryImage, 
				newDetector.eyesInformation.LE, newDetector.eyesInformation.RE);
		newWebcamImagePaint.paintEllipse(&queryImage, 
				newDetector.eyesInformation.LE, newDetector.eyesInformation.RE);
		QImage* qm = QImageIplImageCvt(&queryImage);

		setQImageWebcam(qm);

		cvWaitKey(1);
		// sleep(1);

		delete qm;
	} catch ( std::exception &e) {
		qFatal( "Standard error exception (%s)",
				e.what());
	} catch ( ... ) {
		qFatal( "Unknown exception");
	}
}
// vim: set ts:4 sw:4 et ai

