#include "ofMain.h"

#include "SharingFacesUtils.h"

#define INSTALL

class ofApp : public ofBaseApp {
public:
#ifdef INSTALL
	static const int camWidth = 1920, camHeight = 1080;
	ofxBlackMagic cam;
#else
	static const int camWidth = 1280, camHeight = 720;
	ofVideoGrabber cam;
#endif
	ofShader shader;
	ofxFaceTrackerThreaded tracker;
	BinnedData<FaceTrackerData> data;
	FaceCompare faceCompare;
	MultiThreadedImageSaver imageSaver;
	
	bool useBlackMagic;
	bool rotate;
	int binSize;
	float neighborRadius;
	int neighborCount;
	ofImage rotated;
	
	FaceTrackerData nearestData;
	string lastLabel;
	ofImage similar;
	ofVec3f whitePoint;
	
	Hysteresis presence;
	FadeTimer presenceFade;
	vector< pair<ofVec2f, FaceTrackerData> > currentData;
	
	void setup() {
#ifndef INSTALL
		useSharedData();
#endif
		loadSettings();
		
		tracker.setup();
		tracker.setHaarMinSize(175);
		tracker.setRescale(.25);
		tracker.setIterations(3);
		tracker.setTolerance(2);
		tracker.setClamp(3);
		tracker.setAttempts(4);
		
#ifdef INSTALL
		cam.setup(camWidth, camHeight, 30);
#else
		cam.initGrabber(camWidth, camHeight, false);
#endif
		if(rotate) {
			data.setup(camHeight, camWidth, binSize);
		} else {
			data.setup(camWidth, camHeight, binSize);
		}
		loadMetadata(data);
		presence.setDelay(0, 4);
		presenceFade.setLength(4, .1);
		presenceFade.start();
		shader.load("shaders/colorbalance.vs", "shaders/colorbalance.fs");
		ofDisableAntiAliasing();
		glPointSize(2);
		ofSetLineWidth(3);
		ofSetLogLevel(OF_LOG_VERBOSE);
	}
	void exit() {
		imageSaver.exit();
		tracker.stopThread();
#ifdef INSTALL
		cam.close();
#endif
	}
	void loadSettings() {
#ifdef INSTALL
		rotate = true;
#else
		rotate = false;
#endif
		binSize = 10;
		neighborRadius = 20;
		neighborCount = 100;
	}
	void update() {
#ifdef INSTALL
		if(cam.update()) {
			ofPixels& pixels = cam.getColorPixels();
#else
		cam.update();
		if(cam.isFrameNew()) {
			ofPixels& pixels = cam.getPixelsRef();
#endif
			// next two could be replaced with one line
			ofxCv::rotate90(pixels, rotated, rotate ? 270 : 0);
			ofxCv:flip(rotated, rotated, 1);
			Mat rotatedMat = toCv(rotated);
			if(tracker.update(rotatedMat))  {
				ofVec2f position = tracker.getPosition();
				vector<FaceTrackerData*> neighbors = data.getNeighborsCount(position, neighborCount);
				FaceTrackerData curData;
				curData.load(tracker);
				if(!neighbors.empty()) {
					nearestData = *faceCompare.nearest(curData, neighbors);
					if(nearestData.label != lastLabel) {
						similar.loadImage(nearestData.getImageFilename());
#ifdef INSTALL
						whitePoint = getWhitePoint(similar);
#else
						whitePoint.set(1, 1, 1);
#endif
					}
					lastLabel = nearestData.label;
				}
				if(faceCompare.different(curData, currentData) && faceCompare.different(curData, neighbors)) {
					saveFace(curData, rotated);
					currentData.push_back(pair<ofVec2f, FaceTrackerData>(position, curData));
				}
			}
			presence.update(tracker.getFound());
			if(presence.wasTriggered()) {
				presenceFade.stop();
			}
			if(presence.wasUntriggered()) {
				for(int i = 0; i < currentData.size(); i++) {
					data.add(currentData[i].first, currentData[i].second);
				}
				currentData.clear();
				presenceFade.start();
			}
		}
	}
	void draw() {
		ofBackground(255);
		CGDisplayHideCursor(NULL);
		ofSetColor(255);
		if(similar.isAllocated()) {
			shader.begin();
			shader.setUniformTexture("tex", similar, 0);
			shader.setUniform3fv("whitePoint", (float*) &whitePoint);
			similar.draw(0, 0);
			shader.end();
		}
		ofPushStyle();
		if(presenceFade.getActive()) {
			ofSetColor(0, ofMap(presenceFade.get(), 0, 1, 0, 128));
			ofFill();
			ofRect(0, 0, ofGetWidth(), ofGetHeight());
			ofSetColor(255, ofMap(presenceFade.get(), 0, 1, 0, 32));
			data.drawBins();
			ofSetColor(255, ofMap(presenceFade.get(), 0, 1, 0, 64));
			data.drawData();
		}
		ofSetColor(255, 64);
		ofNoFill();
		if(!tracker.getFound()) {
			ofCircle(tracker.getPosition(), 10);
		}
		tracker.draw();
		ofPopStyle();
		
#ifndef INSTALL
		drawFramerate();
#endif
	}
	void keyPressed(int key) {
		if(key == 'f') {
			ofToggleFullscreen();
		}
	}
	
	void saveFace(FaceTrackerData& data, ofImage& img) {
		string basePath = ofGetTimestampString("%Y.%m.%d/%H.%M.%S.%i");
		data.save("metadata/" + basePath + ".face");
		imageSaver.saveImage(img.getPixelsRef(), data.getImageFilename());
	}
};

#include "ofAppGlutWindow.h"

int main() {
#ifdef INSTALL
	ofSetupOpenGL(ofPtr<ofAppBaseWindow>(new ofAppGlutWindow()), 1080, 1920, OF_FULLSCREEN);
//	ofSetupOpenGL(1080, 1920, OF_FULLSCREEN);
#else
	ofSetupOpenGL(ofPtr<ofAppBaseWindow>(new ofAppGlutWindow()), 1280, 720, OF_WINDOW);
//	ofSetupOpenGL(1280, 720, OF_WINDOW);
#endif
	ofRunApp(new ofApp());
}