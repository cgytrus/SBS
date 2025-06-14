#pragma once

void applyParallax(GJBaseGameLayer* gl);
void cleanupParallax();

CCGLProgram* getDebugShaderFor(CCGLProgram* shader);
CCGLProgram* getOrigShaderFor(CCGLProgram* shader);
