#include "mesh.h"
#include <iostream>
Mesh::Mesh(const std::vector<Vertex> &vertices,
           const std::vector<unsigned int> &indices,
           bool willBeUpdated) : vertices(vertices), indices(indices), willBeUpdated (willBeUpdated)
{
    //vao care retine starea meshei
    glGenVertexArrays(1, &vaoHandle);
    glBindVertexArray(vaoHandle);

    glGenBuffers(NUM_VBOS, vboHandles);

    //trimit GPU-ului vertecsii
    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[VERTEX]);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof (Vertex)*vertices.size(),
                 &vertices[0],
                 willBeUpdated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    //trimit GPU-ului indecsii
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboHandles[INDEX]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 sizeof (unsigned int)*indices.size(),
                 &indices[0],
                 willBeUpdated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    numberOfTriangles = indices.size();

    //ii descriu shader-ului datele trimise

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2*sizeof(glm::vec3)));
    glEnableVertexAttribArray (3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2*sizeof(glm::vec3) + sizeof (glm::vec2)));
    glEnableVertexAttribArray (4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3*sizeof(glm::vec3) + sizeof (glm::vec2)));

    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

Mesh::Mesh (bool willBeUpdated) : numberOfTriangles (0) {
    //vao care retine starea meshei
    glGenVertexArrays(1, &vaoHandle);
    glBindVertexArray(vaoHandle);

    glGenBuffers(NUM_VBOS, vboHandles);

    //trimit GPU-ului vertecsii
    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[VERTEX]);
    glBufferData(GL_ARRAY_BUFFER,
                 0,
                 NULL,
                 willBeUpdated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    //trimit GPU-ului indecsii
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboHandles[INDEX]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 0,
                 NULL,
                 willBeUpdated ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    //ii descriu shader-ului datele trimise
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2*sizeof(glm::vec3)));
    glEnableVertexAttribArray (3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2*sizeof(glm::vec3) + sizeof (glm::vec2)));
    glEnableVertexAttribArray (4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3*sizeof(glm::vec3) + sizeof (glm::vec2)));

    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

Mesh::~Mesh()
{
    glDeleteBuffers(NUM_VBOS, vboHandles);
    glDeleteVertexArrays(1, &vaoHandle);
}

GLuint Mesh::getVao(){
    return vaoHandle;
}

GLsizei Mesh::getNumberOfTriangles(){
    return numberOfTriangles;
}

Mesh* Mesh::getRectangle(){
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back(Vertex(glm::vec3(-1, 1, 0), glm::vec3(0, 1, 0), glm::vec2(0, 1)));
    vertices.push_back(Vertex(glm::vec3(-1, -1, 0), glm::vec3(0, 1, 0), glm::vec2(0, 0)));
    vertices.push_back(Vertex(glm::vec3(1, -1, 0), glm::vec3(0, 1, 0), glm::vec2(1, 0)));
    vertices.push_back(Vertex(glm::vec3(1, 1, 0), glm::vec3(0, 1, 0), glm::vec2(1, 1)));

    indices.push_back(0);
    indices.push_back(1);
    indices.push_back(3);
    indices.push_back(1);
    indices.push_back(2);
    indices.push_back(3);

    return new Mesh(vertices, indices);
}

Mesh* Mesh::getRectangleYUp() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back(Vertex(glm::vec3(-1, 0, 1), glm::vec3(0, 1, 0), glm::vec2(0, 0)));
    vertices.push_back(Vertex(glm::vec3(-1, 0, -1), glm::vec3(0, 1, 0), glm::vec2(0, 1)));
    vertices.push_back(Vertex(glm::vec3(1, 0, -1), glm::vec3(0, 1, 0), glm::vec2(1, 1)));
    vertices.push_back(Vertex(glm::vec3(1, 0, 1), glm::vec3(0, 1, 0), glm::vec2(1, 0)));

    indices.push_back(0);
    indices.push_back(3);
    indices.push_back(1);
    indices.push_back(1);
    indices.push_back(3);
    indices.push_back(2);

    return new Mesh(vertices, indices);
}

void Mesh::draw(){
    glBindVertexArray(getVao());
    glDrawElements(GL_TRIANGLES, numberOfTriangles, GL_UNSIGNED_INT, (void*)0);
    glBindVertexArray(0);
}

Mesh *Mesh::getSurface(int width, int height){
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float dw = 2.0f / (float)width;
    float dh = 2.0f / (float)height;
		//creeaza suprafata pentru apa
    for(int i = 0; i < height; i++){
        for(int j = 0; j < width; j++){
            int first_index = 4*(j + i*width);
            vertices.push_back(Vertex(glm::vec3(-1.0f + j * dw, 0.0f, 1.0f - i * dh), glm::vec3(0, 1, 0)));
            vertices.push_back(Vertex(glm::vec3(-1.0f + j * dw, 0.0f, 1.0f - (i+1) * dh), glm::vec3(0, 1, 0)));
            vertices.push_back(Vertex(glm::vec3(-1.0f + (j+1) * dw, 0.0f, 1.0f - (i+1) * dh), glm::vec3(0, 1, 0)));
            vertices.push_back(Vertex(glm::vec3(-1.0f + (j+1) * dw, 0.0f, 1.0f - i * dh), glm::vec3(0, 1, 0)));

            indices.push_back(first_index+2);
            indices.push_back(first_index+1);
            indices.push_back(first_index+0);

            indices.push_back(first_index);
            indices.push_back(first_index+3);
            indices.push_back(first_index+2);

        }
    }
    return new Mesh(vertices, indices);
}

Mesh *Mesh::getDome(int x, int y){
    float pi = 3.1415;
    float phiUpperBound = pi / 2.0;
    float thetaUpperBound = 2 * pi;

    int numOfPointsY = y;
    int numOfPointsXZ = x;

    float stepY = phiUpperBound / (float)(numOfPointsY-1);
    float stepXZ = thetaUpperBound / (float)(numOfPointsXZ-1);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for(int i = 0; i < numOfPointsXZ; i++){
        float theta = (float)i*stepXZ;
        for(int j = 0; j < numOfPointsY; j++){
            float phi = (float)j*stepY;

            float x = glm::cos(theta) * glm::sin(phi) * 0.5f;
            float z = glm::sin(theta) * glm::sin(phi) * 0.5f;
            float y = glm::cos(phi) * 0.5f;

            float u = theta / thetaUpperBound;
            float v = phi / phiUpperBound;

            vertices.push_back(Vertex(glm::vec3(x, y, z), glm::vec2(u, v)));

            int index = numOfPointsY * i + j;
            int nextIndex = (numOfPointsY * (i+1)) + j;

            if(j < numOfPointsY-1 && i < numOfPointsXZ-1){
                indices.push_back(index);
                indices.push_back(nextIndex);
                indices.push_back(index+1);

                indices.push_back(index+1);
                indices.push_back(nextIndex);
                indices.push_back(nextIndex+1);}
        }
    }

    int index = (numOfPointsXZ-1) * numOfPointsY;
    int index2 = (numOfPointsXZ * numOfPointsY);

    for(int i = 0; i < numOfPointsY-1; i++){
        float phi = (float)i*stepY;

        float x = glm::sin(phi) * 0.5f;
        float z = 0;
        float y = glm::cos(phi) * 0.5f;

        float v = phi / phiUpperBound;

        vertices.push_back(Vertex(glm::vec3(x, y, z), glm::vec2(1.0, v)));

        indices.push_back(index+i);
        indices.push_back(index2+i);
        indices.push_back(index+i+1);

        indices.push_back(index+i+1);
        indices.push_back(index2+i);
        indices.push_back(index2+i+1);
    }

    vertices.push_back(Vertex(glm::vec3(0.5, 0, 0), glm::vec2(1.0, 1.0)));

    return new Mesh(vertices, indices);
}

Mesh *Mesh::getCircle(float x, float y, float radius, int numOfTriangles){
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    vertices.push_back(Vertex(glm::vec3(x, y, 0)));
    float step = 360.0f/numOfTriangles;

    int index = 1;

    for(float theta = 0; theta < 360.0f; theta += step){
        float _x1 = radius * glm::cos(glm::radians(theta)) + x;
        float _y1 = radius * glm::sin(glm::radians(theta)) + y;

        float _x2 = radius * glm::cos(glm::radians(theta + step)) + x;
        float _y2 = radius * glm::sin(glm::radians(theta + step)) + y;
        vertices.push_back(Vertex(glm::vec3(_x1, _y1, 0)));
        vertices.push_back(Vertex(glm::vec3(_x2, _y2, 0)));

        indices.push_back(index);
        indices.push_back(0);
        indices.push_back(index+1);
        index += 2;
    }


    return new Mesh(vertices, indices);
}

float Mesh::_stringToFloat(const std::string &source){
		std::stringstream ss(source.c_str());
		float result;
		ss>>result;
		return result;
}
//transforms a string to an int
unsigned int Mesh::_stringToUint(const std::string &source){
    std::stringstream ss(source.c_str());
    unsigned int result;
    ss>>result;
    return result;
}
//transforms a string to an int
int Mesh::_stringToInt(const std::string &source){
    std::stringstream ss(source.c_str());
    int result;
    ss>>result;
    return result;
}
//writes the tokens of the source string into tokens
void Mesh::_stringTokenize(const std::string &source, std::vector<std::string> &tokens){
    tokens.clear();
    std::string aux=source;
    for(unsigned int i=0;i<aux.size();i++) if(aux[i]=='\t' || aux[i]=='\n') aux[i]=' ';
    std::stringstream ss(aux,std::ios::in);
    while(ss.good()){
        std::string s;
        ss>>s;
        if(s.size()>0) tokens.push_back(s);
    }
}
//variant for faces

void Mesh::_faceTokenize(const std::string &source, std::vector<std::string> &tokens){
    std::string aux=source;
    for(unsigned int i=0;i<aux.size();i++) if(aux[i]=='\\' || aux[i]=='/') aux[i]=' ';
    _stringTokenize(aux,tokens);
}

Mesh* Mesh::loadObject(std::string filename){
    /* TODO indices created by this functions are not really useful,
        they simply replicate triangles, without reusing vertices
    */

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    float maxValue = std::numeric_limits<float>::max();
    float minValue = -maxValue;
    glm::vec3 _minCoordinates = glm::vec3(maxValue);
    glm::vec3 _maxCoordinates = glm::vec3(minValue);

    std::cout << "Loading " << filename << std::endl;

    std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
		if(!file.good()){
			std::cout<<"Mesh Loader: Nu am gasit fisierul obj "<<filename<<" sau nu am drepturile sa il deschid!"<<std::endl;
			std::terminate();
		}

		std::string line;
		std::vector<std::string> tokens, facetokens;
		std::vector<glm::vec3> positions;		positions.reserve(1000);
		std::vector<glm::vec3> normals;		normals.reserve(1000);
		std::vector<glm::vec2> texcoords;		texcoords.reserve(1000);
		while(std::getline(file,line)){
			//tokenizeaza linie citita
			_stringTokenize(line,tokens);

			//daca nu am nimic merg mai departe
			if(tokens.size()==0) continue;

			//daca am un comentariu merg mai departe
			if(tokens.size()>0 && tokens[0].at(0)=='#') continue;

			//daca am un vertex
			if(tokens.size()>3 && tokens[0]=="v") {
			    float _x = _stringToFloat(tokens[1]);
                float _y = _stringToFloat(tokens[2]);
                float _z = _stringToFloat(tokens[3]);
                positions.push_back(glm::vec3(_x, _y, _z ));
                if (_x < _minCoordinates.x) {
                    _minCoordinates.x = _x;
                }
                if (_y < _minCoordinates.y) {
                    _minCoordinates.y = _y;
                }
                if (_z < _minCoordinates.z) {
                    _minCoordinates.z =_z;
                }
                if (_x > _maxCoordinates.x) {
                    _maxCoordinates.x = _x;
                }
                if (_y > _maxCoordinates.y) {
                    _maxCoordinates.y = _y;
                }
                if (_z > _maxCoordinates.z) {
                    _maxCoordinates.z =_z;
                }
			}
			//daca am o normala
			if(tokens.size()>3 && tokens[0]=="vn") normals.push_back(glm::vec3(_stringToFloat(tokens[1]), _stringToFloat(tokens[2]), _stringToFloat(tokens[3]) ));

			//daca am un texcoord
			if(tokens.size()>2 && tokens[0]=="vt") texcoords.push_back(glm::vec2(_stringToFloat(tokens[1]), _stringToFloat(tokens[2]) ));

			//daca am o fata (f+ minim 3 indecsi)
			if(tokens.size()>=4 && tokens[0]=="f"){

				//foloseste primul vertex al fetei pentru a determina formatul fetei (v v/t v//n v/t/n) = (1 2 3 4)
				unsigned int face_format = 0;
				if(tokens[1].find("//")!=std::string::npos) face_format = 3;
				_faceTokenize(tokens[1],facetokens);
				if(facetokens.size()==3) face_format =4; // vertecsi/texcoords/normale
				else{
					if(facetokens.size()==2){
						if(face_format !=3) face_format=2;	//daca nu am vertecsi/normale am vertecsi/texcoords
					}else{
						face_format =1; //doar vertecsi
					}
				}

				//primul index din acest poligon
				unsigned int index_of_first_vertex_of_face = -1;


				for(unsigned int num_token =1; num_token<tokens.size();num_token++){
					if(tokens[num_token].at(0)=='#') break;					//comment dupa fata
					_faceTokenize(tokens[num_token],facetokens);
					if(face_format==1){
						//doar pozitie
						int p_index = _stringToInt(facetokens[0]);
						if(p_index>0) p_index -=1;								//obj has 1...n indices
						else p_index = positions.size()+p_index;				//index negativ

						vertices.push_back(Vertex(glm::vec3(positions[p_index].x, positions[p_index].y, positions[p_index].z)));
					}else if(face_format==2){
						// pozitie si texcoord
						int p_index = _stringToInt(facetokens[0]);
						if(p_index>0) p_index -=1;								//obj has 1...n indices
						else p_index = positions.size()+p_index;				//index negativ

						int t_index = _stringToInt(facetokens[1]);
						if(t_index>0) t_index -=1;								//obj has 1...n indices
						else t_index = texcoords.size()+t_index;				//index negativ

						vertices.push_back(Vertex(glm::vec3(positions[p_index].x, positions[p_index].y, positions[p_index].z),glm::vec2(texcoords[t_index].x, texcoords[t_index].y)));
					}else if(face_format==3){
						//pozitie si normala
						int p_index = _stringToInt(facetokens[0]);
						if(p_index>0) p_index -=1;								//obj has 1...n indices
						else p_index = positions.size()+p_index;				//index negativ

						int n_index = _stringToInt(facetokens[1]);
						if(n_index>0) n_index -=1;								//obj has 1...n indices
						else n_index = normals.size()+n_index;					//index negativ

						vertices.push_back(Vertex(glm::vec3(positions[p_index].x, positions[p_index].y, positions[p_index].z), glm::vec3(normals[n_index].x, normals[n_index].y, normals[n_index].z)));
					}else{
						//pozitie normala si texcoord
						int p_index = _stringToInt(facetokens[0]);
						if(p_index>0) p_index -=1;								//obj has 1...n indices
						else p_index = positions.size()+p_index;				//index negativ

						int t_index = _stringToInt(facetokens[1]);
						if(t_index>0) t_index -=1;								//obj has 1...n indices
						else t_index = normals.size()+t_index;					//index negativ

						int n_index = _stringToInt(facetokens[2]);
						if(n_index>0) n_index -=1;								//obj has 1...n indices
						else n_index = normals.size()+n_index;					//index negativ

						vertices.push_back(Vertex(glm::vec3(positions[p_index].x, positions[p_index].y, positions[p_index].z),glm::vec3(normals[n_index].x, normals[n_index].y, normals[n_index].z), glm::vec2(texcoords[t_index].x, texcoords[t_index].y)));
					}

					//adauga si indecsii
					if(num_token<4){
						if(num_token == 1) index_of_first_vertex_of_face = vertices.size()-1;
						//doar triunghiuri f 0 1 2 3 (4 indecsi, primul e ocupat de f)
						indices.push_back(vertices.size()-1);
					}else{
						//polygon => triunghi cu ultimul predecesor vertexului nou adaugat si 0 relativ la vertecsi poligon(independent clockwise)
						indices.push_back(index_of_first_vertex_of_face);
						indices.push_back(vertices.size()-2);
						indices.push_back(vertices.size()-1);
					}
				}//end for

				/* try and calculate tangent and bitangent for face vertices */
                assert (tokens.size() == 4); /* only triangles */
                int numOfVertices = vertices.size();
                computeTangentAndBi (vertices[numOfVertices-3],
                                     vertices[numOfVertices-2],
                                     vertices[numOfVertices-1]);
			}//end face

		}//end while
		return new Mesh(vertices, indices);
}

void Mesh::computeTangentAndBi(Vertex &v1, Vertex &v2, Vertex &v3) {
    glm::vec3 &p1 = v1.positionCoords;
    glm::vec3 &p2 = v2.positionCoords;
    glm::vec3 &p3 = v3.positionCoords;

    glm::vec2 &uv1 = v1.textureCoords;
    glm::vec2 &uv2 = v2.textureCoords;
    glm::vec2 &uv3 = v3.textureCoords;

    glm::vec3 deltaP1 = p2-p1;
    glm::vec3 deltaP2 = p3-p1;
    glm::vec2 deltaUV1 = uv2-uv1;
    glm::vec2 deltaUV2 = uv3-uv1;

    float det = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
    if (det == 0) {
        return;
    }

    float r = 1.0f / det;
    glm::vec3 tangent = glm::normalize ((deltaP1 * deltaUV2.y - deltaP2 * deltaUV1.y) * r);

    v1.tangent = tangent;
    v2.tangent = tangent;
    v3.tangent = tangent;
    v1.biTangent = glm::normalize (glm::cross (v1.normalCoords, tangent));
    v2.biTangent = glm::normalize (glm::cross (v2.normalCoords, tangent));
    v3.biTangent = glm::normalize (glm::cross (v3.normalCoords, tangent));

    /*std::cout << "face:" << std::endl;
    std::cout << "vertex:" << v1.positionCoords.x * 2 << " " << v1.positionCoords.y * 2<< " " << v1.positionCoords.z*2 << std::endl;
    std::cout << "vertex:" << v2.positionCoords.x * 2 << " " << v2.positionCoords.y * 2<< " " << v2.positionCoords.z*2 << std::endl;
    std::cout << "vertex:" << v3.positionCoords.x * 2 << " " << v3.positionCoords.y * 2<< " " << v3.positionCoords.z*2 << std::endl;

    std::cout << "v1" << std::endl;
    std::cout << "tangent=" << tangent.x << " " << tangent.y << " " << tangent.z << std::endl;
    std::cout << "bitangent=" << v1.biTangent.x << " " << v1.biTangent.y << " " << v1.biTangent.z << std::endl;
    std::cout << "normal=" << v1.normalCoords.x << " " << v1.normalCoords.y << " " << v1.normalCoords.z << std::endl;

    std::cout << "v2" << std::endl;
    std::cout << "tangent=" << tangent.x << " " << tangent.y << " " << tangent.z << std::endl;
    std::cout << "bitangent=" << v2.biTangent.x << " " << v2.biTangent.y << " " << v2.biTangent.z << std::endl;
    std::cout << "normal=" << v2.normalCoords.x << " " << v2.normalCoords.y << " " << v2.normalCoords.z << std::endl;

    std::cout << "v3" << std::endl;
    std::cout << "tangent=" << tangent.x << " " << tangent.y << " " << tangent.z << std::endl;
    std::cout << "bitangent=" << v3.biTangent.x << " " << v3.biTangent.y << " " << v3.biTangent.z << std::endl;
    std::cout << "normal=" << v3.normalCoords.x << " " << v3.normalCoords.y << " " << v3.normalCoords.z << std::endl;*/
}

void Mesh::update (const std::vector<Vertex> &vertices,
                   const std::vector<unsigned int> &indices) {

    assert (willBeUpdated);

    glBindBuffer(GL_ARRAY_BUFFER, vboHandles[VERTEX]);
    glBufferSubData(GL_ARRAY_BUFFER,
                    0,
                    sizeof (Vertex)*vertices.size(),
                    &vertices[0]);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboHandles[INDEX]);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                    0,
                    sizeof (unsigned int)*indices.size(),
                    &indices[0]);

    numberOfTriangles = indices.size();

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}
