#include <unordered_set>

#include <spdlog/spdlog.h>
#include <imgui/imgui.h>

#include "REFramework.hpp"

#include "FirstPerson.hpp"

FirstPerson* g_firstPerson = nullptr;

FirstPerson::FirstPerson() {
    // thanks imgui
    g_firstPerson = this;
    m_attachBoneImgui.reserve(256);

    // Specific player model configs
    // Leon
    m_attachOffsets["pl0000"] = Vector4f{ -0.26f, 0.435f, 1.25f, 0.0f };
    // Claire
    m_attachOffsets["pl1000"] = Vector4f{ -0.23f, 0.4f, 1.0f, 0.0f };
    // Sherry
    m_attachOffsets["pl3000"] = Vector4f{ -0.278f, 0.435f, 0.945f, 0.0f };
    // Hunk
    m_attachOffsets["pl4000"] = Vector4f{ -0.26f, 0.435f, 1.25f, 0.0f };
}

void FirstPerson::onFrame() {
    if (!m_enabled) {
        return;
    }

    if (m_cameraSystem == nullptr || m_cameraSystem->ownerGameObject == nullptr) {
        ComponentTraverser::refreshComponents();
        reset();
        return;
    }

    if (!updatePointersFromCameraSystem(m_cameraSystem)) {
        reset();
        return;
    }

    if (m_attachNames.empty()) {
        auto& joints = m_playerTransform->joints;

        for (int32_t i = 0; joints.data != nullptr && i < joints.size; ++i) {
            auto joint = joints.data->joints[i];

            if (joint == nullptr || joint->info == nullptr || joint->info->name == nullptr) {
                continue;
            }

            auto name = std::wstring{ joint->info->name };
            m_attachNames.push_back(std::string{ std::begin(name), std::end(name) }.c_str());
        }
    }
}

void FirstPerson::onDrawUI() {
    ImGui::Begin("FirstPerson");

    ImGui::Checkbox("Enabled", &m_enabled);
    ImGui::SliderFloat3("offset", (float*)&m_attachOffsets[m_playerName], -2.0f, 2.0f, "%.3f", 1.0f);
    ImGui::SliderFloat("CameraScale", &m_scale, 0.0f, 250.0f);
    ImGui::SliderFloat("BoneScale", &m_boneScale, 0.0f, 250.0f);

    if (ImGui::InputText("Joint", m_attachBoneImgui.data(), 256)) {
        m_attachBone = std::wstring{ std::begin(m_attachBoneImgui), std::end(m_attachBoneImgui) };
        reset();
    }

    if (ImGui::Button("Refresh Joints")) {
        m_attachNames.clear();
    }

    static auto listBoxHandler = [](void* data, int idx, const char** outText) -> bool {
        return g_firstPerson->listBoxHandlerAttach(data, idx, outText);
    };

    if (ImGui::ListBox("Joints", &m_attachSelected, listBoxHandler, &m_attachNames, (int32_t)m_attachNames.size())) {
        m_attachBoneImgui = m_attachNames[m_attachSelected];
        m_attachBone = std::wstring{ std::begin(m_attachNames[m_attachSelected]), std::end(m_attachNames[m_attachSelected]) };
        reset();
    }

    ImGui::End();
}

void FirstPerson::onComponent(REComponent* component) {
    auto gameObject = component->ownerGameObject;

    if (gameObject == nullptr || component->info == nullptr || component->info->classInfo == nullptr || component->info->classInfo->type == nullptr) {
        return;
    }

    if (component->info->classInfo->type->name == nullptr) {
        return;
    }

    auto typeName = std::string_view{ component->info->classInfo->type->name };

    //spdlog::info("{:p} {} {}", (void*)component, utility::REString::getString(gameObject->name).c_str(), component->info->classInfo->type->name);

    if (typeName == "app.ropeway.camera.CameraSystem") {
        if (utility::REString::equals(gameObject->name, L"Main Camera")) {

            if (m_cameraSystem != (RopewayCameraSystem*)component) {
                spdlog::info("Found CameraSystem {:p}", (void*)component);
            }

            m_cameraSystem = (RopewayCameraSystem*)component;
        }
    }
}

void FirstPerson::onUpdateTransform(RETransform* transform) {
    if (!m_enabled || m_camera == nullptr || m_camera->ownerGameObject == nullptr) {
        return;
    }

    if (m_playerCameraController == nullptr || m_playerTransform == nullptr || m_cameraSystem == nullptr || m_cameraSystem->cameraController == nullptr) {
        return;
    }

    // can change to action camera
    if (m_cameraSystem->cameraController->activeCamera != m_playerCameraController) {
        return;
    }

    if (transform == m_camera->ownerGameObject->transform) {
        updateCameraTransform(transform);
    }
    else if (transform == m_playerTransform) {
        updatePlayerTransform(transform);
    }
}

void FirstPerson::onUpdateCameraController(RopewayPlayerCameraController* controller) {
    if (!m_enabled || controller->activeCamera != m_playerCameraController || m_playerTransform == nullptr) {
        return;
    }

    auto& boneMatrix = utility::RETransform::getJointMatrix(*m_playerTransform, m_attachBone);

    auto offset = glm::extractMatrixRotation(boneMatrix) * (m_attachOffsets[m_playerName] * Vector4f{ -0.1f, 0.1f, 0.1f, 0.0f });
    auto& bonePos = boneMatrix[3];
    auto finalPos = Vector4f{ bonePos + offset };

    // The following code fixes inaccuracies between the rotation set by the game and what's set in updateCameraTransform
    controller->worldPosition = finalPos;
    *(glm::quat*)&controller->worldRotation = glm::quat{ m_lastCameraMatrix };

    m_lastCameraMatrix[3] = finalPos;
    m_camera->ownerGameObject->transform->worldTransform = m_lastCameraMatrix;
    m_camera->ownerGameObject->transform->angles = *(Vector4f*)&controller->worldRotation;
}

void FirstPerson::onUpdateCameraController2(RopewayPlayerCameraController* controller) {
    if (!m_enabled || controller->activeCamera != m_playerCameraController || m_playerTransform == nullptr) {
        return;
    }

    // Save the original position and rotation before our modifications.
    // If we don't, the camera rotation will freeze up, because it keeps getting overwritten.
    m_lastControllerPos = controller->worldPosition;
    m_lastControllerRotation = *(glm::quat*)&controller->worldRotation;
}

void FirstPerson::reset() {
    m_rotationOffset = glm::identity<decltype(m_rotationOffset)>();
    m_lastBoneRotation = glm::identity<decltype(m_lastBoneRotation)>();
    m_lastCameraMatrix = glm::identity<decltype(m_lastCameraMatrix)>();
    m_lastControllerPos = Vector4f{};
    m_lastControllerRotation = glm::quat{};
    m_updateTimes.clear();
    m_deltaTimes.clear();
}

bool FirstPerson::updatePointersFromCameraSystem(RopewayCameraSystem* cameraSystem) {
    if (cameraSystem == nullptr) {
        return false;
    }

    if (m_camera = cameraSystem->mainCamera; m_camera == nullptr) {
        m_playerTransform = nullptr;
        return false;
    }

    auto joint = cameraSystem->playerJoint;
    
    if (joint == nullptr) {
        m_playerTransform = nullptr;
        return false;
    }

    // Update player name and log it
    if (m_playerTransform != joint->parentTransform && joint->parentTransform != nullptr) {
        if (joint->parentTransform->ownerGameObject == nullptr) {
            return false;
        }

        m_playerName = utility::REString::getString(joint->parentTransform->ownerGameObject->name);

        if (m_playerName.empty()) {
            return false;
        }

        spdlog::info("Found Player {:s} {:p}", m_playerName.data(), (void*)joint->parentTransform);
    }

    // Update player transform pointer
    if (m_playerTransform = joint->parentTransform; m_playerTransform == nullptr) {
        return false;
    }

    // Update PlayerCameraController camera pointer
    if (m_playerCameraController == nullptr) {
        auto controller = cameraSystem->cameraController;

        if (controller == nullptr || controller->ownerGameObject == nullptr || controller->activeCamera == nullptr || controller->activeCamera->ownerGameObject == nullptr) {
            return false;
        }

        if (utility::REString::equals(controller->activeCamera->ownerGameObject->name, L"PlayerCameraController")) {
            m_playerCameraController = controller->activeCamera;
            spdlog::info("Found PlayerCameraController {:p}", (void*)m_playerCameraController);
        }

        return m_playerCameraController != nullptr;
    }

    return true;
}

void FirstPerson::updateCameraTransform(RETransform* transform) {
    auto deltaTime = updateDeltaTime(transform);

    auto& mtx = transform->worldTransform;
    auto& cameraPos = mtx[3];

    auto camPos3 = Vector3f{ m_lastControllerPos };

    auto& boneMatrix = utility::RETransform::getJointMatrix(*m_playerTransform, m_attachBone);
    auto& bonePos = boneMatrix[3];

    auto camRotMat = glm::extractMatrixRotation(Matrix4x4f{ m_lastControllerRotation });
    auto headRotMat = glm::extractMatrixRotation(boneMatrix);

    auto& camForward3 = *(Vector3f*)&camRotMat[2];

    auto offset = headRotMat * (m_attachOffsets[m_playerName] * Vector4f{ -0.1f, 0.1f, 0.1f, 0.0f });
    auto finalPos = Vector3f{ bonePos + offset };

    // Average the distance to the wanted rotation
    auto dist = (glm::distance(m_lastBoneRotation[0], headRotMat[0])
               + glm::distance(m_lastBoneRotation[1], headRotMat[1])
               + glm::distance(m_lastBoneRotation[2], headRotMat[2])) / 3.0f;

    // interpolate the bone rotation (it's snappy otherwise)
    m_lastBoneRotation = glm::interpolate(m_lastBoneRotation, headRotMat, deltaTime * m_boneScale * dist);

    // Look at where the camera is pointing from the head position
    camRotMat = glm::extractMatrixRotation(glm::rowMajor4(glm::lookAtLH(finalPos, camPos3 + (camForward3 * 8192.0f), { 0.0f, 1.0f, 0.0f })));
    // Follow the bone rotation, but rotate towards where the camera is looking.
    auto wantedMat = glm::inverse(m_lastBoneRotation) * camRotMat;

    // Average the distance to the wanted rotation
    dist = (glm::distance(m_rotationOffset[0], wantedMat[0])
          + glm::distance(m_rotationOffset[1], wantedMat[1])
          + glm::distance(m_rotationOffset[2], wantedMat[2])) / 3.0f;

    m_rotationOffset = glm::interpolate(m_rotationOffset, wantedMat, m_scale * deltaTime * dist);
    auto finalMat = m_lastBoneRotation * m_rotationOffset;
    auto finalQuat = glm::quat{ finalMat };

    // Apply the same matrix data to other things stored in-game (positions/quaternions)
    cameraPos = Vector4f{ finalPos, 1.0f };
    m_cameraSystem->cameraController->worldPosition = *(Vector4f*)&cameraPos;
    m_cameraSystem->cameraController->worldRotation = *(Vector4f*)&finalQuat;
    transform->position = *(Vector4f*)&cameraPos;
    transform->angles = *(Vector4f*)&finalQuat;

    // Apply the new matrix
    *(Matrix3x4f*)&mtx = finalMat;
    m_lastCameraMatrix = mtx;
}

void FirstPerson::updatePlayerTransform(RETransform* transform) {
    auto deltaTime = updateDeltaTime(transform);
}

float FirstPerson::updateDeltaTime(REComponent* component) {
    auto deltaDuration = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - m_updateTimes[component]);
    auto deltaTime = deltaDuration.count();

    m_updateTimes[component] = std::chrono::high_resolution_clock::now();
    m_deltaTimes[component] = deltaTime;

    return deltaTime;
}

