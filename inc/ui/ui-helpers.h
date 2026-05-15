#ifndef UI_HELPERS_H_
#define UI_HELPERS_H_

#include <opencv2/opencv.hpp>
#include <GLFW/glfw3.h> // GLFW включает базовые заголовки OpenGL

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

inline GLuint mat_to_texture(const cv::Mat& mat, GLuint existing_texture = 0)
{
    if (mat.empty()) return existing_texture;

    GLuint image_texture = existing_texture;
    
    if (image_texture == 0)
    {
        glGenTextures(1, &image_texture);
    }

    glBindTexture(GL_TEXTURE_2D, image_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mat.cols, mat.rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, mat.data);

    return image_texture;
}

#endif /* UI_HELPERS_H_ */