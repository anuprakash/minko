/*
Copyright (c) 2014 Aerys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "minko/Minko.hpp"
#include "minko/MinkoSDL.hpp"
#include "minko/audio/PositionalSound.hpp"
#include "minko/audio/SoundParser.hpp"
#include "minko/log/Logger.hpp"

using namespace minko;
using namespace minko::component;
using namespace minko::animation;

int
main(int argc, char** argv)
{
    auto canvas = Canvas::create("Minko Example - Audio");

    auto sceneManager = SceneManager::create(canvas);

    // setup assets
    sceneManager->assets()->loader()->options()
        ->resizeSmoothly(true)
        ->generateMipmaps(true)
        ->registerParser<audio::SoundParser>("ogg");

    auto errorHandle = sceneManager->assets()->loader()->error()->connect([=](file::Loader::Ptr loader, const file::Error& error)
    {
        LOG_ERROR(error.what());
    });

    sceneManager->assets()->loader()
        ->queue("audio/breakbeat.ogg")
        ->queue("effect/Basic.effect");

    auto root = scene::Node::create("root")
        ->addComponent(sceneManager);

    auto mesh = scene::Node::create("mesh")
        ->addComponent(Transform::create());

    auto camera = scene::Node::create("camera")
        ->addComponent(Renderer::create(0x7f7f7fff))
        ->addComponent(Transform::create(
            math::inverse(
                math::lookAt(
                    math::vec3(0.f, 0.f, 3.f), math::vec3(), math::vec3(0, 1, 0)
                )
            )
        ))
        ->addComponent(PerspectiveCamera::create(canvas->aspectRatio()));

    root->addChild(camera);
    root->addChild(mesh);
    
    std::vector<uint> timetable = { 0, 2000, 4000 };

    std::vector<math::mat4> matrices = {
        math::translate(math::vec3(-5, 0, 0)),
        math::translate(math::vec3(5, 0, 0)),
        math::translate(math::vec3(-5, 0, 0))
    };

    auto timeline = Matrix4x4Timeline::create("transform.matrix", 4000, timetable, matrices, true);

    auto _ = sceneManager->assets()->loader()->complete()->connect([=](file::Loader::Ptr loader)
    {
        auto cubeMaterial =  material::BasicMaterial::create();
        cubeMaterial->diffuseColor(0xffff00ff);
        
        mesh->addComponent(Surface::create(
                geometry::CubeGeometry::create(sceneManager->assets()->context()),
                cubeMaterial,
                sceneManager->assets()->effect("effect/Basic.effect")
            )
        );

        mesh->addComponent(Animation::create({ timeline })->play());
        
        audio::Sound::Ptr sound = sceneManager->assets()->sound("audio/breakbeat.ogg");
        audio::SoundChannel::Ptr channel = sound->play(std::numeric_limits<int>::max());

        auto script = audio::PositionalSound::create(channel, camera);
        mesh->addComponent(script);
    });

    auto resized = canvas->resized()->connect([&](AbstractCanvas::Ptr canvas, uint w, uint h)
    {
        camera->component<PerspectiveCamera>()->aspectRatio(float(w) / float(h));
    });

    auto enterFrame = canvas->enterFrame()->connect([&](Canvas::Ptr canvas, float time, float deltaTime)
    {
        mesh->component<Transform>()->matrix(
            math::rotate(0.001f * deltaTime, math::vec3(0.f, 1.f, 0.f)) *
            mesh->component<Transform>()->matrix()
        );

        sceneManager->nextFrame(time, deltaTime);
    });

    sceneManager->assets()->loader()->load();
    canvas->run();

    return 0;
}
