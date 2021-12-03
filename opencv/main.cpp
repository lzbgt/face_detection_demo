#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

#include "event_detection.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/objdetect.hpp"
#include "opencv2/videoio.hpp"
#include "utils/common.hpp"

using namespace std;
using namespace cv;
namespace fs = std::filesystem;

void detectAndDraw(Mat &img, CascadeClassifier &cascade,
                   CascadeClassifier &nestedCascade,
                   double scale, bool showGui);

string cascadeName;
string nestedCascadeName;

int main(int argc, const char **argv) {
    VideoCapture capture;
    Mat frame, image;
    string inputName;
    bool tryflip;
    CascadeClassifier cascade, nestedCascade;
    double scale;

    auto modPath = fs::path(myutils::Common::GetModulePath()).parent_path().string();

    cv::CommandLineParser parser(argc, argv,
                                 "{help h||}"
                                 "{show s||}"
                                 "{cascade|data/haarcascades/haarcascade_frontalface_alt.xml|}"
                                 "{nested-cascade|data/haarcascades/haarcascade_eye_tree_eyeglasses.xml|}"
                                 "{scale|1|}{try-flip||}{@filename||}");

    bool guiShow = parser.has("show");
    auto pathPrefix = std::string(modPath.begin(), modPath.end()) + std::string("\\");
    cascadeName = pathPrefix + parser.get<string>("cascade");
    nestedCascadeName = pathPrefix + parser.get<string>("nested-cascade");
    scale = parser.get<double>("scale");
    if (scale < 1)
        scale = 1;
    tryflip = parser.has("try-flip");
    inputName = parser.get<string>("@filename");
    if (!parser.check()) {
        parser.printErrors();
        return 0;
    }
    if (!nestedCascade.load(samples::findFileOrKeep(nestedCascadeName)))
        cerr << "WARNING: Could not load classifier cascade for nested objects" << endl;
    if (!cascade.load(samples::findFile(cascadeName))) {
        cerr << "ERROR: Could not load classifier cascade" << endl;
        return -1;
    }
    if (inputName.empty() || (isdigit(inputName[0]) && inputName.size() == 1)) {
        int camera = inputName.empty() ? 0 : inputName[0] - '0';
        if (!capture.open(camera)) {
            cout << "Capture from camera #" << camera << " didn't work" << endl;
            return 1;
        }
        capture.set(cv::CAP_PROP_BUFFERSIZE, 0);
    } else if (!inputName.empty()) {
        image = imread(samples::findFileOrKeep(inputName), IMREAD_COLOR);
        if (image.empty()) {
            if (!capture.open(samples::findFileOrKeep(inputName))) {
                cout << "Could not read " << inputName << endl;
                return 1;
            }
        }
    } else {
        cerr << "ERROR: no input specified" << endl;
    }

    if (capture.isOpened()) {
        cout << "Video capturing has been started ..." << endl;
        EventDetection::getInstance().run();

        for (;;) {
            capture >> frame;
            if (frame.empty())
                break;

            Mat frame1 = frame.clone();
            detectAndDraw(frame1, cascade, nestedCascade, scale, guiShow);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (guiShow) {
                char c = (char)waitKey(10);
                if (c == 27 || c == 'q' || c == 'Q')
                    break;
            }
        }
    } else {
        cout << "Detecting face(s) in " << inputName << endl;
        if (!image.empty()) {
            detectAndDraw(image, cascade, nestedCascade, scale, true);
            waitKey(0);
        } else if (!inputName.empty()) {
            /* assume it is a text file containing the
            list of the image filenames to be processed - one per line */
            FILE *f = fopen(inputName.c_str(), "rt");
            if (f) {
                char buf[1000 + 1];
                while (fgets(buf, 1000, f)) {
                    int len = (int)strlen(buf);
                    while (len > 0 && isspace(buf[len - 1]))
                        len--;
                    buf[len] = '\0';
                    cout << "file " << buf << endl;
                    image = imread(buf, 1);
                    if (!image.empty()) {
                        detectAndDraw(image, cascade, nestedCascade, scale, tryflip);
                        char c = (char)waitKey(0);
                        if (c == 27 || c == 'q' || c == 'Q')
                            break;
                    } else {
                        cerr << "Aw snap, couldn't read image " << buf << endl;
                    }
                }
                fclose(f);
            }
        }
    }

    return 0;
}

void detectAndDraw(Mat &img, CascadeClassifier &cascade,
                   CascadeClassifier &nestedCascade,
                   double scale, bool guiShow) {
    static auto lastEvent = std::chrono::system_clock::now();
    static bool hasEvent = false;
    static int cnt = 0;
    vector<Rect> faces, faces2;
    const static Scalar colors[] =
        {
            Scalar(255, 0, 0),
            Scalar(255, 128, 0),
            Scalar(255, 255, 0),
            Scalar(0, 255, 0),
            Scalar(0, 128, 255),
            Scalar(0, 255, 255),
            Scalar(0, 0, 255),
            Scalar(255, 0, 255)};
    Mat gray, smallImg;

    cvtColor(img, gray, COLOR_BGR2GRAY);
    double fx = 1 / scale;
    resize(gray, smallImg, Size(), fx, fx, INTER_LINEAR_EXACT);
    equalizeHist(smallImg, smallImg);

    auto now = std::chrono::system_clock::now();
    cascade.detectMultiScale(smallImg, faces,
                             1.1, 2, 0
                                         //|CASCADE_FIND_BIGGEST_OBJECT
                                         //|CASCADE_DO_ROUGH_SEARCH
                                         | CASCADE_SCALE_IMAGE,
                             Size(30, 30));

    auto deltaS = std::chrono::duration<double>(now - lastEvent).count();
    std::cout << "delta: " << deltaS << ", cnt: " << cnt << ", faces: " << faces.size() << std::endl;

    if (faces.size() > 0) {
        cnt++;
        if (deltaS >= 1) {
            if (cnt > 5 * deltaS) {
                if (!hasEvent) {
                    hasEvent = true;
                    EventDetection::getInstance().notify(faces.size());
                }
                lastEvent = now;
                cnt = 0;
            }
        }
    } else {
        if (!hasEvent) {
            cnt = 0;
            lastEvent = now;
        }
        if (deltaS >= 3) {
            if (cnt <= 5 * deltaS) {
                if (hasEvent) {
                    EventDetection::getInstance().notify(0);
                    hasEvent = false;
                }
            }
        }
    }

    // printf("detection time = %g ms, got face: %s\n", t * 1000 / getTickFrequency(), faces.size() > 0 ? "true" : "fase");

    if (guiShow) {
        for (size_t i = 0; i < faces.size(); i++) {
            Rect r = faces[i];
            Mat smallImgROI;
            vector<Rect> nestedObjects;
            Point center;
            Scalar color = colors[i % 8];
            int radius;

            double aspect_ratio = (double)r.width / r.height;
            if (0.75 < aspect_ratio && aspect_ratio < 1.3) {
                center.x = cvRound((r.x + r.width * 0.5) * scale);
                center.y = cvRound((r.y + r.height * 0.5) * scale);
                radius = cvRound((r.width + r.height) * 0.25 * scale);
                circle(img, center, radius, color, 3, 8, 0);
            } else
                rectangle(img, Point(cvRound(r.x * scale), cvRound(r.y * scale)),
                          Point(cvRound((r.x + r.width - 1) * scale), cvRound((r.y + r.height - 1) * scale)),
                          color, 3, 8, 0);
            if (nestedCascade.empty())
                continue;
            smallImgROI = smallImg(r);
            nestedCascade.detectMultiScale(smallImgROI, nestedObjects,
                                           1.1, 2, 0
                                                       //|CASCADE_FIND_BIGGEST_OBJECT
                                                       //|CASCADE_DO_ROUGH_SEARCH
                                                       //|CASCADE_DO_CANNY_PRUNING
                                                       | CASCADE_SCALE_IMAGE,
                                           Size(30, 30));
            for (size_t j = 0; j < nestedObjects.size(); j++) {
                Rect nr = nestedObjects[j];
                center.x = cvRound((r.x + nr.x + nr.width * 0.5) * scale);
                center.y = cvRound((r.y + nr.y + nr.height * 0.5) * scale);
                radius = cvRound((nr.width + nr.height) * 0.25 * scale);
                circle(img, center, radius, color, 3, 8, 0);
            }
        }
        imshow("result", img);
    }
}
