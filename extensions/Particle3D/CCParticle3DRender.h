/****************************************************************************
 Copyright (c) 2015-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

 https://adxeproject.github.io/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#pragma once

#include <vector>

#include "renderer/CCRenderState.h"
#include "renderer/backend/Types.h"
#include "renderer/CCMeshCommand.h"
#include "renderer/CCCallbackCommand.h"
#include "renderer/backend/Buffer.h"
#include "base/CCRef.h"
#include "math/CCMath.h"
#include "ExtensionExport.h"

NS_CC_BEGIN

class ParticleSystem3D;
class Renderer;
class MeshCommand;
class Sprite3D;
class GLProgramState;
class IndexBuffer;
class VertexBuffer;
class Texture2D;

/**
 * 3d particle render
 */
class CC_EX_DLL Particle3DRender : public Ref
{
    friend class ParticleSystem3D;

public:
    virtual void render(Renderer* renderer, const Mat4& transform, ParticleSystem3D* particleSystem) = 0;

    /** Perform activities when a Renderer is started.
     */
    virtual void notifyStart();
    /** Perform activities when a Renderer is stopped.
     */
    virtual void notifyStop();
    /** Notify that the Particle System is rescaled.
     */
    virtual void notifyRescaled(const Vec3& scale);

    void setVisible(bool isVisible) { _isVisible = isVisible; }

    bool isVisible() const { return _isVisible; }

    void setDepthTest(bool isDepthTest);
    void setDepthWrite(bool isDepthWrite);
    void setBlendFunc(const BlendFunc& blendFunc);

    void copyAttributesTo(Particle3DRender* render);

    virtual void reset() {}

    Particle3DRender();
    virtual ~Particle3DRender();

protected:
    ParticleSystem3D* _particleSystem;
    RenderState::StateBlock _stateBlock;
    bool _isVisible;
    Vec3 _rendererScale;
    bool _depthTest;
    bool _depthWrite;
};

// particle render for quad
class CC_EX_DLL Particle3DQuadRender : public Particle3DRender
{
public:
    static Particle3DQuadRender* create(std::string_view texFile = "");

    virtual void render(Renderer* renderer, const Mat4& transform, ParticleSystem3D* particleSystem) override;

    virtual void reset() override;
    Particle3DQuadRender();
    virtual ~Particle3DQuadRender();

protected:
    bool initQuadRender(std::string_view texFile);

    void onBeforeDraw();
    void onAfterDraw();

protected:
    MeshCommand _meshCommand;
    CallbackCommand _beforeCommand;
    CallbackCommand _afterCommand;
    Texture2D* _texture                  = nullptr;
    backend::ProgramState* _programState = nullptr;
    backend::Buffer* _indexBuffer        = nullptr;  // index buffer
    backend::Buffer* _vertexBuffer       = nullptr;  // vertex buffer

    struct posuvcolor
    {
        Vec3 position;
        Vec2 uv;
        Vec4 color;
    };

    std::vector<posuvcolor> _posuvcolors;  // vertex data
    std::vector<uint16_t> _indexData;      // index data
    std::string _texFile;

    backend::UniformLocation _locColor;
    backend::UniformLocation _locTexture;
    backend::UniformLocation _locPMatrix;

    // renderer state cache variables
    bool _rendererDepthTestEnabled                 = true;
    backend::CompareFunction _rendererDepthCmpFunc = backend::CompareFunction::LESS;
    backend::CullMode _rendererCullMode            = backend::CullMode::BACK;
    backend::Winding _rendererWinding              = backend::Winding::COUNTER_CLOCK_WISE;
    bool _rendererDepthWrite                       = false;
};

// particle render for Sprite3D
class CC_EX_DLL Particle3DModelRender : public Particle3DRender
{
public:
    static Particle3DModelRender* create(std::string_view modelFile, std::string_view texFile = "");

    virtual void render(Renderer* renderer, const Mat4& transform, ParticleSystem3D* particleSystem) override;

    virtual void reset() override;
    Particle3DModelRender();
    virtual ~Particle3DModelRender();

protected:
    std::vector<Sprite3D*> _spriteList;
    std::string _modelFile;
    std::string _texFile;
    Vec3 _spriteSize;
};

NS_CC_END
