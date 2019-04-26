//=============================================================================================
// Computer Graphics Sample Program: GPU ray casting
//=============================================================================================
#include "framework.h"

// vertex shader in GLSL
const char *vertexSource = R"(
	#version 450
    precision highp float;

	uniform vec3 wLookAt, wRight, wUp;          // pos of eye

	layout(location = 0) in vec2 cCamWindowVertex;	// Attrib Array 0
	out vec3 p;

	void main() {
		gl_Position = vec4(cCamWindowVertex, 0, 1);
		p = wLookAt + wRight * cCamWindowVertex.x + wUp * cCamWindowVertex.y;
	}
)";
// fragment shader in GLSL
const char *fragmentSource = R"(
	#version 450
    precision highp float;

	struct Material {
		vec3 ka, kd, ks;
		float  shininess;
		vec3 F0;
		int rough, reflective;
	};

	struct Light {
		vec3 direction;
		vec3 Le, La;
	};

	struct Sphere {
		vec3 center;
		float radius;
	};

	struct Hit {
		float t;
		vec3 position, normal;
		int mat;	// material index
	};

	struct Ray {
		vec3 start, dir;
	};

    struct Plane{
        vec3 normal;
        vec3 point;
    };

	const int nMaxObjects = 100;
	uniform vec3 wEye;
	uniform Light light;
	uniform Material materials[2];  // diffuse, specular, ambient ref
	uniform int nObjects;
    uniform int nPlanes;
	uniform Sphere objects[nMaxObjects];
    uniform Plane planes[nMaxObjects];

	in  vec3 p;					// point on camera window corresponding to the pixel
	out vec4 fragmentColor;		// output that goes to the raster memory as told by glBindFragDataLocation

	Hit intersect(const Sphere object, const Ray ray) {
		Hit hit;
		hit.t = -1;
		vec3 dist = ray.start - object.center;
		float a = dot(ray.dir, ray.dir);
		float b = dot(dist, ray.dir) * 2.0;
		float c = dot(dist, dist) - object.radius * object.radius;
		float discr = b * b - 4.0 * a * c;
		if (discr < 0) return hit;
		float sqrt_discr = sqrt(discr);
		float t1 = (-b + sqrt_discr) / 2.0 / a;	// t1 >= t2 for sure
		float t2 = (-b - sqrt_discr) / 2.0 / a;
		if (t1 <= 0) return hit;
		hit.t = (t2 > 0) ? t2 : t1;
		hit.position = ray.start + ray.dir * hit.t;
		hit.normal = (hit.position - object.center) / object.radius;
		return hit;
	}
    Hit intersect(const Plane oPlane, const Ray ray){
        Hit hit;
        hit.t = -1;
        float nevezo = dot(ray.dir, oPlane.normal);
        if( nevezo == 0) return hit;
        float szamlalo = dot(oPlane.point-ray.start, oPlane.normal);
        hit.t = szamlalo/nevezo;
		hit.position = ray.start + ray.dir * hit.t;
        if(length(hit.position-oPlane.point) > 3){
            hit.t = -1;
            return hit;
        }
        hit.normal = oPlane.normal;
        return hit;
    }

	Hit firstIntersect(Ray ray) {
		Hit bestHit;
		bestHit.t = -1;
		for (int o = 0; o < nObjects; o++) {
			Hit hit = intersect(objects[o], ray); //  hit.t < 0 if no intersection
            hit.mat = 0;	 // half of the objects are rough
			if (hit.t > 0 && (bestHit.t < 0 || hit.t < bestHit.t))  bestHit = hit;
		}
        for (int o = 0; o < nPlanes; o++) {
			Hit hit = intersect(planes[o], ray); //  hit.t < 0 if no intersection
            hit.mat = 1;     // half of the objects are reflective
			if (hit.t > 0 && (bestHit.t < 0 || hit.t < bestHit.t))  bestHit = hit;
		}
		if (dot(ray.dir, bestHit.normal) > 0) bestHit.normal = bestHit.normal * (-1);
		return bestHit;
	}

	bool shadowIntersect(Ray ray) {	// for directional lights
		for (int o = 0; o < nObjects; o++) if (intersect(objects[o], ray).t > 0) return true;
        for (int o = 0; o < nPlanes; o++) if (intersect(planes[o], ray).t > 0) return true;//  hit.t < 0 if no intersection
		return false;
	}

	vec3 Fresnel(vec3 F0, float cosTheta) {
		return F0 + (vec3(1, 1, 1) - F0) * pow(cosTheta, 5);
	}

	const float epsilon = 0.0001f;
	const int maxdepth = 5;

	vec3 trace(Ray ray) {
		vec3 weight = vec3(1, 1, 1);
		vec3 outRadiance = vec3(0, 0, 0);
		for(int d = 0; d < maxdepth; d++) {
			Hit hit = firstIntersect(ray);
			if (hit.t < 0) return weight * light.La;
			if (materials[hit.mat].rough == 1) {
				outRadiance += weight * materials[hit.mat].ka * light.La;
				Ray shadowRay;
				shadowRay.start = hit.position + hit.normal * epsilon;
				shadowRay.dir = light.direction;
				float cosTheta = dot(hit.normal, light.direction);
				if (cosTheta > 0 && !shadowIntersect(shadowRay)) {
					outRadiance += weight * light.Le * materials[hit.mat].kd * cosTheta;
					vec3 halfway = normalize(-ray.dir + light.direction);
					float cosDelta = dot(hit.normal, halfway);
					if (cosDelta > 0) outRadiance += weight * light.Le * materials[hit.mat].ks * pow(cosDelta, materials[hit.mat].shininess);
				}
			}

			if (materials[hit.mat].reflective == 1) {
				weight *= Fresnel(materials[hit.mat].F0, dot(-ray.dir, hit.normal));
				ray.start = hit.position + hit.normal * epsilon;
				ray.dir = reflect(ray.dir, hit.normal);
			} else return outRadiance;
		}
	}

	void main() {
		Ray ray;
		ray.start = wEye;
		ray.dir = normalize(p - wEye);
		fragmentColor = vec4(trace(ray), 1);
	}
)";

class Material {
protected:
    vec3 ka, kd, ks;
    float  shininess;
    vec3 F0;
    bool rough, reflective;
public:
    Material RoughMaterial(vec3 _kd, vec3 _ks, float _shininess) {
        ka = _kd * M_PI;
        kd = _kd;
        ks = _ks;
        shininess = _shininess;
        rough = true;
        reflective = false;
    }
    Material SmoothMaterial(vec3 _F0) {
        F0 = _F0;
        rough = false;
        reflective = true;
    }
    void SetUniform(unsigned int shaderProg, int mat) {
        char buffer[256];
        sprintf(buffer, "materials[%d].ka", mat);
        ka.SetUniform(shaderProg, buffer);
        sprintf(buffer, "materials[%d].kd", mat);
        kd.SetUniform(shaderProg, buffer);
        sprintf(buffer, "materials[%d].ks", mat);
        ks.SetUniform(shaderProg, buffer);
        sprintf(buffer, "materials[%d].shininess", mat);
        int location = glGetUniformLocation(shaderProg, buffer);
        if (location >= 0) glUniform1f(location, shininess); else printf("uniform material.shininess cannot be set\n");
        sprintf(buffer, "materials[%d].F0", mat);
        F0.SetUniform(shaderProg, buffer);

        sprintf(buffer, "materials[%d].rough", mat);
        location = glGetUniformLocation(shaderProg, buffer);
        if (location >= 0) glUniform1i(location, rough ? 1 : 0); else printf("uniform material.rough cannot be set\n");
        sprintf(buffer, "materials[%d].reflective", mat);
        location = glGetUniformLocation(shaderProg, buffer);
        if (location >= 0) glUniform1i(location, reflective ? 1 : 0); else printf("uniform material.reflective cannot be set\n");
    }
};

class RoughMaterial : public Material {
public:
    RoughMaterial(vec3 _kd, vec3 _ks, float _shininess) {
        ka = _kd * M_PI;
        kd = _kd;
        ks = _ks;
        shininess = _shininess;
        rough = true;
        reflective = false;
    }
};

class SmoothMaterial : public Material {
public:
    SmoothMaterial(vec3 _F0) {
        F0 = _F0;
        rough = false;
        reflective = true;
    }
};

struct Sphere {
    vec3 center;
    float radius;

    Sphere(const vec3& _center, float _radius) { center = _center; radius = _radius; }
    void SetUniform(unsigned int shaderProg, int o) {
        char buffer[256];
        sprintf(buffer, "objects[%d].center", o);
        center.SetUniform(shaderProg, buffer);
        sprintf(buffer, "objects[%d].radius", o);
        int location = glGetUniformLocation(shaderProg, buffer);
        if (location >= 0) glUniform1f(location, radius); else printf("uniform %s cannot be set\n", buffer);
    }
};
struct Plane{
    vec3 normal;
    vec3 point;
    Plane(const vec3 & _normal, const vec3 & _point){normal = _normal; point = _point;}
    void SetUniform(unsigned int shaderProg, int o) {
        char buffer[256];
        sprintf(buffer, "planes[%d].normal", o);
        normal.SetUniform(shaderProg, buffer);
        sprintf(buffer, "planes[%d].point", o);
        point.SetUniform(shaderProg, buffer);
    }

};
class Camera {
    vec3 eye, lookat, right, up;
    float fov;
public:
    void set(vec3 _eye, vec3 _lookat, vec3 vup, double _fov) {
        eye = _eye;
        lookat = _lookat;
        fov = _fov;
        vec3 w = eye - lookat;
        float f = length(w);
        right = normalize(cross(vup, w)) * f * tan(fov / 2);
        up = normalize(cross(w, right)) * f * tan(fov / 2);
    }
    void SetUniform(unsigned int shaderProg) {
        eye.SetUniform(shaderProg, "wEye");
        lookat.SetUniform(shaderProg, "wLookAt");
        right.SetUniform(shaderProg, "wRight");
        up.SetUniform(shaderProg, "wUp");
    }
};

struct Light {
    vec3 direction;
    vec3 Le, La;
    Light(vec3 _direction, vec3 _Le, vec3 _La) {
        direction = normalize(_direction);
        Le = _Le; La = _La;
    }
    void SetUniform(unsigned int shaderProg) {
        La.SetUniform(shaderProg, "light.La");
        Le.SetUniform(shaderProg, "light.Le");
        direction.SetUniform(shaderProg, "light.direction");
    }
};

float rnd() { return (float)rand() / RAND_MAX; }

class Scene {
    int numberOfMirrors = 3;
    std::vector<Sphere *> objects;
    std::vector<Plane *> planes;
    std::vector<Light *> lights;
    Camera camera;
    std::vector<Material *> materials;
public:
    void build() {
        vec3 eye = vec3(0, 0, 2);
        vec3 vup = vec3(0, 1, 0);
        vec3 lookat = vec3(0, 0, 0);
        float fov = 45 * M_PI / 180;
        camera.set(eye, lookat, vup, fov);

        lights.push_back(new Light(vec3(0, 0, 4), vec3(3, 3, 3), vec3(0.4, 0.3, 0.3)));

        vec3 kd(0.3f, 0.2f, 0.1f), ks(10, 10, 10);
        //objects.push_back(new Sphere(vec3(rnd() - 0.5, rnd() - 0.5, rnd() - 0.5), rnd()*0.1 ));
        objects.push_back(new Sphere(vec3(0, 0, -10), 0.2 ));
        //planes.push_back(new Plane(vec3(0,1,0), vec3(0,-1,-3)));
        //planes.push_back(new Plane(vec3(0,-1,0), vec3(0,1,-3)));
       // planes.push_back(new Plane(vec3(-1,0,0), vec3(1,0,-3)));
        //planes.push_back(new Plane(vec3(1,0,0), vec3(-1,0,-3)));
        numberOfMirrors = 3;
        float centralAngle = 2*M_PI/numberOfMirrors;
        float currentAngle = 0;
        for(int i = 0; i < numberOfMirrors; i++){
            vec2 position;
            position.x = sin(currentAngle);
            position.y = cos(currentAngle);
            planes.push_back(new Plane(vec3(-position.x, -position.y, 0), vec3(position.x, position.y, -3)));
            currentAngle += centralAngle;
        }


        materials.push_back(new RoughMaterial(kd, ks, 50));
        materials.push_back(new SmoothMaterial(vec3(0.9, 0.85, 0.8)));
    }
    void SetUniform(unsigned int shaderProg) {
        {
            int location = glGetUniformLocation(shaderProg, "nObjects");
            if (location >= 0) glUniform1i(location, objects.size()); else printf("uniform nObjects cannot be set\n");
        }
        {
            int location = glGetUniformLocation(shaderProg, "nPlanes");
            if (location >= 0) glUniform1i(location, planes.size()); else printf("uniform nObjects cannot be set\n");
        }
        for (int o = 0; o < objects.size(); o++) objects[o]->SetUniform(shaderProg, o);
        for (int o = 0; o < planes.size(); o++) planes[o]->SetUniform(shaderProg, o);
        lights[0]->SetUniform(shaderProg);
        camera.SetUniform(shaderProg);
        for (int mat = 0; mat < materials.size(); mat++) materials[mat]->SetUniform(shaderProg, mat);
    }
    void increaseMirrorNumber(){
        for(int i = 0; i < numberOfMirrors; i++){
            planes.pop_back();
        }
        numberOfMirrors++;
        float centralAngle = 2*M_PI/numberOfMirrors;
        float currentAngle = 0;
        for(int i = 0; i < numberOfMirrors; i++){
            vec2 position;
            position.x = sin(currentAngle);
            position.y = cos(currentAngle);
            planes.push_back(new Plane(vec3(-position.x, -position.y, 0), vec3(position.x, position.y, -3)));
            currentAngle += centralAngle;
        }
    }
    void Animate(float dt) { /*camera.Animate(dt);*/ }
};

GPUProgram gpuProgram; // vertex and fragment shaders
Scene scene;

class FullScreenTexturedQuad {
    unsigned int vao;	// vertex array object id and texture id
public:
    void Create() {
        glGenVertexArrays(1, &vao);	// create 1 vertex array object
        glBindVertexArray(vao);		// make it active

        unsigned int vbo;		// vertex buffer objects
        glGenBuffers(1, &vbo);	// Generate 1 vertex buffer objects

        // vertex coordinates: vbo0 -> Attrib Array 0 -> vertexPosition of the vertex shader
        glBindBuffer(GL_ARRAY_BUFFER, vbo); // make it active, it is an array
        float vertexCoords[] = { -1, -1,  1, -1,  1, 1,  -1, 1 };	// two triangles forming a quad
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);	   // copy to that part of the memory which is not modified
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);     // stride and offset: it is tightly packed
    }

    void Draw() {
        glBindVertexArray(vao);	// make the vao and its vbos active playing the role of the data source
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);	// draw two triangles forming a quad
    }
};

FullScreenTexturedQuad fullScreenTexturedQuad;

// Initialization, create an OpenGL context
void onInitialization() {
    glViewport(0, 0, windowWidth, windowHeight);
    scene.build();
    fullScreenTexturedQuad.Create();

    // create program for the GPU
    gpuProgram.Create(vertexSource, fragmentSource, "fragmentColor");
    gpuProgram.Use();
}

// Window has become invalid: Redraw
void onDisplay() {
    static int nFrames = 0;
    nFrames++;
    static long tStart = glutGet(GLUT_ELAPSED_TIME);
    long tEnd = glutGet(GLUT_ELAPSED_TIME);
    printf("%d msec\r", (tEnd - tStart) / nFrames);

    glClearColor(1.0f, 0.5f, 0.8f, 1.0f);							// background color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear the screen
    scene.SetUniform(gpuProgram.getId());
    fullScreenTexturedQuad.Draw();
    glutSwapBuffers();									// exchange the two buffers
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
    switch(key){
        case 'a':
            scene.increaseMirrorNumber();
            break;
        default:
            break;
    }

}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) {
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {
}

// Idle event indicating that some time elapsed: do animation here
void onIdle() {
    scene.Animate(0.01);
    glutPostRedisplay();
}
