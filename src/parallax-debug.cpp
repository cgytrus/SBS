#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include "parallax.hpp"

void toggleDebugModeHooks(auto& self) {
    if (Mod::get()->getSettingValue<bool>("debug"))
        return;
    for (const auto& [ name, hook ] : self.m_hooks) {
        hook->setAutoEnable(false);
        if (!hook->disable())
            log::warn("uhh failed to disable debug hook, enjoy free debug mode i guess");
    }
}
#define TOGGLE_DEBUG_MODE_HOOKS toggleDebugModeHooks(self)

const char* PositionTextureColorVert = R"(
attribute vec4 a_position;
attribute vec2 a_texCoord;
attribute vec4 a_color;

#ifdef GL_ES
varying lowp vec4 v_fragmentColor;
varying mediump vec2 v_texCoord;
#else
varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
#endif
varying float v_depth;

void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_fragmentColor = a_color;
    v_texCoord = a_texCoord;
    v_depth = a_position.z;
}
)";
const char* PositionTextureColorFrag = R"(
#ifdef GL_ES
precision lowp float;
#endif

varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
uniform sampler2D CC_Texture0;
varying float v_depth;

void main() {
    gl_FragColor = texture2D(CC_Texture0, v_texCoord) * v_fragmentColor;
    gl_FragColor = vec4(clamp(v_depth, 0.0, 1.0), max(abs(v_depth) - 1.0, 0.0), clamp(-v_depth, 0.0, 1.0), 1.0) * gl_FragColor.a;
}
)";

const char* PositionTextureColorAlphaTestFrag = R"(
#ifdef GL_ES
precision lowp float;
#endif

varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
uniform sampler2D CC_Texture0;
uniform float CC_alpha_value;
varying float v_depth;

void main() {
    vec4 texColor = texture2D(CC_Texture0, v_texCoord);
    if (texColor.a <= CC_alpha_value)
        discard;
    gl_FragColor = texColor * v_fragmentColor;
    gl_FragColor = vec4(clamp(v_depth, 0.0, 1.0), max(abs(v_depth) - 1.0, 0.0), clamp(-v_depth, 0.0, 1.0), 1.0) * gl_FragColor.a;
}
)";

const char* PositionColorVert = R"(
attribute vec4 a_position;
attribute vec4 a_color;

#ifdef GL_ES
varying lowp vec4 v_fragmentColor;
#else
varying vec4 v_fragmentColor;
#endif
varying float v_depth;

void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_fragmentColor = a_color;
    v_depth = a_position.z;
}
)";
const char* PositionColorFrag = R"(
#ifdef GL_ES
precision lowp float;
#endif

varying vec4 v_fragmentColor;
varying float v_depth;

void main() {
    gl_FragColor = v_fragmentColor;
    gl_FragColor = vec4(clamp(v_depth, 0.0, 1.0), max(abs(v_depth) - 1.0, 0.0), clamp(-v_depth, 0.0, 1.0), 1.0) * gl_FragColor.a;
}
)";

const char* PositionUColorVert = R"(
attribute vec4 a_position;
uniform	vec4 u_color;
uniform float u_pointSize;

#ifdef GL_ES
varying lowp vec4 v_fragmentColor;
#else
varying vec4 v_fragmentColor;
#endif
varying float v_depth;

void main() {
    gl_Position = CC_MVPMatrix * a_position;
    gl_PointSize = u_pointSize;
    v_fragmentColor = u_color;
    v_depth = a_position.z;
}
)";

const char* CustomProgramFrag = R"(
#ifdef GL_ES
precision lowp float;
#endif

varying vec2 v_texCoord;
uniform sampler2D CC_Texture0;
varying float v_depth;

void main() {
    gl_FragColor = texture2D(CC_Texture0, v_texCoord);
    //gl_FragColor = vec4(clamp(v_depth, 0.0, 1.0), max(abs(v_depth) - 1.0, 0.0), clamp(-v_depth, 0.0, 1.0), 1.0) * gl_FragColor.a;
}
)";

std::unordered_map<CCGLProgram*, CCGLProgram*> s_normalToDebug;
std::unordered_map<CCGLProgram*, std::string> s_normalToKey;
std::unordered_set<CCGLProgram*> s_debugs;

#include <Geode/modify/CCShaderCache.hpp>
class $modify(DebugShaderCache, CCShaderCache) {
    static void onModify(auto& self) {
        TOGGLE_DEBUG_MODE_HOOKS;
    }

    static DebugShaderCache* get() {
        return static_cast<DebugShaderCache*>(sharedShaderCache());
    }

    $override void destructor() {
        CCShaderCache::~CCShaderCache();
        s_normalToDebug.clear();
        s_normalToKey.clear();
        s_debugs.clear();
    }

    CCGLProgram* createOrResetDebugShader(CCGLProgram* normal) {
        bool exists = s_normalToDebug.contains(normal);
        CCGLProgram*& debug = s_normalToDebug[normal];
        if (exists) {
            debug->reset();
            return debug;
        }
        debug = new CCGLProgram();
        s_debugs.emplace(debug);
        return debug;
    }

    CCGLProgram* createOrResetDebugShader(const char* normal) {
        return this->createOrResetDebugShader(this->programForKey(normal));
    }

    CCGLProgram* getDebugShaderFor(CCGLProgram* program) {
        if (s_normalToDebug.contains(program))
            return s_normalToDebug[program];
        if (s_debugs.contains(program))
            return program;
        if (!s_normalToKey.contains(program)) {
            log::warn("parallax debug shader missing! breakpoint me to find out which");
            return program;
        }
        // ignore for now
        if (s_normalToKey[program] == "custom_program")
            return program;
        log::warn("parallax debug shader missing! {}", s_normalToKey[program]);
        return program;
    }

    void initShaders() {
        auto programs = CCDictionaryExt<std::string, CCGLProgram*>(m_pPrograms);
        for (const auto& [ key, program ] : programs) {
            this->addProgram(program, key.c_str());
        }
    }

    $override void loadDefaultShaders() {
        CCShaderCache::loadDefaultShaders();
        initShaders();
    }

    $override void reloadDefaultShaders() {
        CCShaderCache::reloadDefaultShaders();
        initShaders();
    }

    $override void addProgram(CCGLProgram* program, const char* key) {
        CCShaderCache::addProgram(program, key);
        const std::string k = key;
        s_normalToKey[program] = k;
        if (k == kCCShader_PositionTextureColor) {
            CCGLProgram* p = this->createOrResetDebugShader(key);
            p->initWithVertexShaderByteArray(
                PositionTextureColorVert,
                PositionTextureColorFrag
            );
            p->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
            p->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
            p->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
            p->link();
            p->updateUniforms();
            return;
        }
        if (k == kCCShader_PositionTextureColorAlphaTest) {
            CCGLProgram* p = this->createOrResetDebugShader(key);
            p->initWithVertexShaderByteArray(
                PositionTextureColorVert,
                PositionTextureColorAlphaTestFrag
            );
            p->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
            p->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
            p->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
            p->link();
            p->updateUniforms();
            return;
        }
        if (k == kCCShader_PositionColor) {
            CCGLProgram* p = this->createOrResetDebugShader(key);
            p->initWithVertexShaderByteArray(
                PositionColorVert,
                PositionColorFrag
            );
            p->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
            p->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
            p->link();
            p->updateUniforms();
            return;
        }
        if (k == kCCShader_Position_uColor) {
            CCGLProgram* p = this->createOrResetDebugShader(key);
            p->initWithVertexShaderByteArray(
                PositionUColorVert,
                PositionColorFrag
            );
            p->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
            p->link();
            p->updateUniforms();
            return;
        }
        if (k == "custom_program") {
            CCGLProgram* p = this->createOrResetDebugShader(key);
            p->initWithVertexShaderByteArray(
                PositionTextureColorVert,
                CustomProgramFrag
            );
            p->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
            p->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
            p->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
            p->link();
            p->updateUniforms();
            return;
        }
    }
};

#define FUNNY_CCGLPROGRAM_OVERRIDE(Name, ...) \
    if (parallax::s_debug) \
        return DebugShaderCache::get()->getDebugShaderFor(this)->Name(__VA_ARGS__); \
    return CCGLProgram::Name(__VA_ARGS__)

#include <Geode/modify/CCGLProgram.hpp>
class $modify(CCGLProgram) {
    static void onModify(auto& self) {
        TOGGLE_DEBUG_MODE_HOOKS;
    }

    $override void use() {
        FUNNY_CCGLPROGRAM_OVERRIDE(use);
    }

    $override void updateUniforms() {
        FUNNY_CCGLPROGRAM_OVERRIDE(updateUniforms);
    }

    $override GLint getUniformLocationForName(const char* name) {
        FUNNY_CCGLPROGRAM_OVERRIDE(getUniformLocationForName, name);
    }

    $override void setUniformLocationWith1i(GLint location, GLint i1) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith1i, location, i1);
    }

    $override void setUniformLocationWith2i(GLint location, GLint i1, GLint i2) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith2i, location, i1, i2);
    }

    $override void setUniformLocationWith3i(GLint location, GLint i1, GLint i2, GLint i3) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith3i, location, i1, i2, i3);
    }

    $override void setUniformLocationWith4i(GLint location, GLint i1, GLint i2, GLint i3, GLint i4) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith4i, location, i1, i2, i3, i4);
    }

    $override void setUniformLocationWith2iv(GLint location, GLint* ints, unsigned int numberOfArrays) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith2iv, location, ints, numberOfArrays);
    }

    $override void setUniformLocationWith3iv(GLint location, GLint* ints, unsigned int numberOfArrays) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith3iv, location, ints, numberOfArrays);
    }

    $override void setUniformLocationWith4iv(GLint location, GLint* ints, unsigned int numberOfArrays) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith4iv, location, ints, numberOfArrays);
    }

    $override void setUniformLocationWith1f(GLint location, GLfloat f1) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith1f, location, f1);
    }

    $override void setUniformLocationWith2f(GLint location, GLfloat f1, GLfloat f2) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith2f, location, f1, f2);
    }

    $override void setUniformLocationWith3f(GLint location, GLfloat f1, GLfloat f2, GLfloat f3) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith3f, location, f2, f2, f3);
    }

    $override void setUniformLocationWith4f(GLint location, GLfloat f1, GLfloat f2, GLfloat f3, GLfloat f4) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith4f, location, f1, f2, f3, f4);
    }

    $override void setUniformLocationWith2fv(GLint location, GLfloat* floats, unsigned int numberOfArrays) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith2fv, location, floats, numberOfArrays);
    }

    $override void setUniformLocationWith3fv(GLint location, GLfloat* floats, unsigned int numberOfArrays) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith3fv, location, floats, numberOfArrays);
    }

    $override void setUniformLocationWith4fv(GLint location, GLfloat* floats, unsigned int numberOfArrays) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWith4fv, location, floats, numberOfArrays);
    }

    $override void setUniformLocationWithMatrix4fv(GLint location, GLfloat* matrixArray, unsigned int numberOfMatrices) {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformLocationWithMatrix4fv, location, matrixArray, numberOfMatrices);
    }

    $override void setUniformsForBuiltins() {
        FUNNY_CCGLPROGRAM_OVERRIDE(setUniformsForBuiltins);
    }
};
