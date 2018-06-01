#ifndef TEXT_H
#define TEXT_H

#include "Font.h"
#include "Mesh.h"
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Shader.h"

class Text
{
    public:
        Text (Font &font,
              const glm::vec3 &position,
              const glm::vec3 &color,
              const std::string &text = "",
              float size = 10,
              const glm::vec3 &rotation = glm::vec3 (0));
        Mesh &getMesh();
        void setColor(glm::vec3 color);
        void setPosition(glm::vec3 pos);
        const glm::vec3 &getPosition();
        void move(glm::vec3 dr);
        void changeSize(int size);
        void addLetter(char letter);
        void deleteLetter ();
        void displayNumber (long number);
        void display (const std::string& str);
        void displayDouble (double number, int prec = 3);
        void draw (Shader &shader);
        const std::string &getCurrentString();
        ~Text();
    protected:
    private:
        Font &m_font;
        Mesh mesh;
        glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 color;
        float size;
        std::string currentString;
        glm::mat4 modelMatrix;
        bool isModelMatrixModified;

        void computeModelMatrix ();
        void updateMesh (bool);
};

#endif // TEXT_H
