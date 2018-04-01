#include "text.h"

#include <math.h>

/* TODO add transform to this class */
Text::Text(Font &font,
           const glm::vec3 &position,
           const glm::vec3 &color,
           const std::string &text,
           float size,
           const glm::vec3 &rotation) : mesh (true),
                                        currentString (text),
                                        m_font (font),
                                        position (position),
                                        rotation (rotation),
                                        color (color),
                                        size (size),
                                        modelMatrix (1.0)
{
    updateMesh (0);
    computeModelMatrix ();
}

void Text::updateMesh(bool dir){
    /* TODO pretty slow, update just what needs to be updated */

    std::vector<Character> chars = m_font.getChars();
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::vec2 cursor(0);

    for(int i = (dir == 0 ? 0 : currentString.size()); dir == 0 ? (i < currentString.size()) : (i >= 0); dir == 0 ? i++ : i--){
        char letter = currentString[i];
        int id = (int)letter;
        Character charInfo = chars[id];
        int index = i*4;

        int fontWidth = m_font.getWidth();
        int fontHeight = m_font.getHeight();

        float x, y, width, height;

        x = (float)(cursor.x + charInfo.xoffset) / fontWidth;
        y = (float)(cursor.y + charInfo.yoffset) / fontHeight;
        width = (float) charInfo.width / fontWidth;
        height = (float) charInfo.height/ fontHeight;
        cursor.x += (float) charInfo.xadvance;

        float x_text = (float) charInfo.x / fontWidth;
        float y_text = (float) charInfo.y / fontHeight;
        float width_text = (float) charInfo.width / fontWidth;
        float height_text = (float) charInfo.height / fontHeight;

        vertices.push_back(Vertex(glm::vec3(x, y-height, 0.0), glm::vec3(0,0,1), glm::vec2(x_text, (y_text + height_text))));
        vertices.push_back(Vertex(glm::vec3(x+width, y-height, 0.0), glm::vec3(0,0,1), glm::vec2(x_text + width_text, (y_text + height_text))));
        vertices.push_back(Vertex(glm::vec3(x, y, 0.0), glm::vec3(0,0,1), glm::vec2(x_text, y_text)));
        vertices.push_back(Vertex(glm::vec3(x+width, y, 0.0), glm::vec3(0,0,1), glm::vec2(x_text + width_text, y_text)));

        indices.push_back(index);
        indices.push_back(index+1);
        indices.push_back(index+2);
        indices.push_back(index+2);
        indices.push_back(index+1);
        indices.push_back(index+3);
    }

    if (vertices.size () != 0 && indices.size () != 0) {
        mesh.update (vertices, indices);
    }
}

void Text::computeModelMatrix(){
    modelMatrix = glm::translate(glm::mat4(1.0), position);
    modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1,0,0));
    modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0,1,0));
    modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0,0,1));
    modelMatrix = glm::scale(modelMatrix, glm::vec3(size*10));

    isModelMatrixModified = false;
}

void Text::changeSize(int dsize){
    size += dsize;
    if(size < 1){
        size = 1;
    }else if(size > 20){
        size = 20;
    }
}

void Text::addLetter(char letter){
    currentString.push_back(letter);
    updateMesh(0);
}

void Text::displayNumber(long number){
    currentString.clear();
    while(number){
        currentString.push_back((number % 10)+'0');
        number /= 10;
    }
    updateMesh(1);
}

void Text::displayDouble (double number, int prec) {
    int leftOfDot, rightOfDot;
    double fracPart;
    double intPart;

    fracPart = modf (number, &intPart);

    leftOfDot = (int) intPart;
    rightOfDot = (int) (fracPart * pow (10, prec));

    currentString.clear ();
    if (rightOfDot == 0) {
        currentString.push_back ('0');
    } else {
        while (rightOfDot != 0) {
            currentString.push_back ((rightOfDot % 10) + '0');
            rightOfDot /= 10;
        }
    }
    currentString.push_back ('.');
    if (leftOfDot == 0) {
        currentString.push_back ('0');
    } else {
        while (leftOfDot != 0) {
            currentString.push_back ((leftOfDot % 10) + '0');
            leftOfDot /= 10;
        }
    }

    if (currentString.size () > 1) {
        updateMesh (1);
    }
}

void Text::display(const std::string& str) {
    currentString = str;
    updateMesh(0);
}

void Text::deleteLetter() {
    currentString = currentString.substr(0, currentString.size()-1);
    updateMesh(0);
}

void Text::draw(Shader &shader) {
    bool result = true;

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (isModelMatrixModified) {
        computeModelMatrix ();
    }

    result &= shader.updateUniform("modelMatrix", (void *) &modelMatrix);
    result &= shader.updateUniform("color", (void *) &color);
    assert (result);

    shader.bind();

    glActiveTexture (GL_TEXTURE0);
    glEnable (GL_TEXTURE_2D);
    glBindTexture (GL_TEXTURE_2D, m_font.getTextureId ());

    mesh.draw ();

    shader.unbind();

    glDisable (GL_BLEND);
}

Mesh &Text::getMesh() {
    return mesh;
}

void Text::setColor(glm::vec3 color) {
    this->color = color;
}

void Text::setPosition(glm::vec3 pos) {
    position = pos;
    isModelMatrixModified = true;
}

const glm::vec3 &Text::getPosition() {
    return position;
}

void Text::move(glm::vec3 dr) {
    position += dr;
    isModelMatrixModified = true;
}

Text::~Text() {

}
