#ifdef DEBUG
#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include "parallax.hpp"

CCGLProgram* s_positionTextureColorOrig = nullptr;
CCGLProgram* s_positionTextureColor = nullptr;
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

void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_fragmentColor = a_color;
    v_texCoord = a_texCoord;
}
)";
const char* PositionTextureColorFrag = R"(
#ifdef GL_ES
precision lowp float;
#endif

varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
uniform sampler2D CC_Texture0;

void main() {
    vec4 color = texture2D(CC_Texture0, v_texCoord) * v_fragmentColor;
    if (color.a < 1.0)
        discard;
    gl_FragColor = color;
}
)";

CCGLProgram* s_positionColorOrig = nullptr;
CCGLProgram* s_positionColor = nullptr;
const char* PositionColorVert = R"(
attribute vec4 a_position;
attribute vec4 a_color;

#ifdef GL_ES
varying lowp vec4 v_fragmentColor;
#else
varying vec4 v_fragmentColor;
#endif

void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_fragmentColor = a_color;
}
)";
const char* PositionColorFrag = R"(
#ifdef GL_ES
precision lowp float;
#endif

varying vec4 v_fragmentColor;

void main() {
    vec4 color = v_fragmentColor;
    if (color.a < 1.0)
        discard;
    gl_FragColor = color;
}
)";

#include <Geode/modify/CCShaderCache.hpp>
class $modify(CCShaderCache) {
    void initShaders() {
        s_positionTextureColorOrig = this->programForKey(kCCShader_PositionTextureColor);
        s_positionColorOrig = this->programForKey(kCCShader_PositionColor);

        s_positionTextureColor->initWithVertexShaderByteArray(
            PositionTextureColorVert,
            PositionTextureColorFrag
        );
        s_positionTextureColor->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
        s_positionTextureColor->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
        s_positionTextureColor->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
        s_positionTextureColor->link();
        s_positionTextureColor->updateUniforms();

        s_positionColor->initWithVertexShaderByteArray(
            PositionColorVert,
            PositionColorFrag
        );
        s_positionColor->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
        s_positionColor->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
        s_positionColor->link();
        s_positionColor->updateUniforms();
    }

    $override void loadDefaultShaders() {
        CCShaderCache::loadDefaultShaders();
        s_positionTextureColor = new CCGLProgram();
        s_positionColor = new CCGLProgram();
        initShaders();
    }

    $override void reloadDefaultShaders() {
        CCShaderCache::reloadDefaultShaders();
        s_positionTextureColor->reset();
        s_positionColor->reset();
        initShaders();
    }
};

CCGLProgram* getDebugShaderFor(CCGLProgram* shader) {
    if (shader == s_positionTextureColorOrig)
        return s_positionTextureColor;
    if (shader == s_positionColorOrig)
        return s_positionColor;
    log::warn("parallax debug shader missing! breakpoint me to find out which");
    return shader;
}

CCGLProgram* getOrigShaderFor(CCGLProgram* shader) {
    if (shader == s_positionTextureColor)
        return s_positionTextureColorOrig;
    if (shader == s_positionColor)
        return s_positionColorOrig;
    //log::warn("orig shader missing! breakpoint me to find out which");
    return shader;
}
#endif
