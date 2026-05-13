#include <windows.h>
#include <capturer/module.h>

cv::Mat capture_screen_to_mat() 
{
    // Получаем размеры экрана
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    // Стандартный WinAPI захват (GDI)
    HWND hwnd = GetDesktopWindow();
    HDC hdcWindow = GetDC(hwnd);
    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);
    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcWindow, width, height);
    SelectObject(hdcMemDC, hbmScreen);

    // Копируем экран в контекст памяти
    BitBlt(hdcMemDC, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);

    // Создаем матрицу OpenCV
    cv::Mat mat(height, width, CV_8UC4); // 8-bit, 4 channels (BGRA)
    
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;  // Отрицательное, чтобы изображение не было перевернутым
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    // Копируем биты из WinAPI Bitmap в cv::Mat
    GetDIBits(hdcWindow, hbmScreen, 0, height, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // Чистим ресурсы WinAPI
    DeleteObject(hbmScreen);
    DeleteDC(hdcMemDC);
    ReleaseDC(hwnd, hdcWindow);

    return mat;
}

void run_capture_test() 
{
    cv::namedWindow("Stream Preview", cv::WINDOW_NORMAL);
    cv::resizeWindow("Stream Preview", 800, 600);

    while (true) {
        cv::Mat frame = capture_screen_to_mat();
        
        // Показываем кадр в окне
        cv::imshow("Stream Preview", frame);

        // Выход при нажатии Esc
        if (cv::waitKey(1) == 27) break;
    }
    cv::destroyAllWindows();
}