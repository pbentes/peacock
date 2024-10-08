#include "Scene.h"

#include <Components/UUID.h>
#include <Components/Transform.h>
#include <Components/Tag.h>
#include <Components/Node.h>
#include <Components/SceneComponent.h>

#include "Entity.h"
#include "EntityTemplates.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Engine {
    std::unordered_map<uint64_t, Scene*> s_ActiveScenes;

    Scene::Scene(const std::string& name, bool isEditorScene, bool initalize): m_Name(name), m_IsEditorScene(isEditorScene) {
        m_SceneEntity = m_Registry.create();
        m_Registry.emplace<SceneComponent>(m_SceneEntity, m_SceneID);
        s_ActiveScenes[m_SceneID] = this;

        if (!initalize)
			return;
    }
    
    Scene::~Scene() {
        m_Registry.clear();
        s_ActiveScenes.erase(this->m_SceneID);
    }

    Entity Scene::CreateEntity(const std::string& name) {
        return CreateChildEntity({}, name);
    }

    Entity Scene::CreateChildEntity(Entity parent, const std::string& name) {
        auto entity = Entity{ m_Registry.create(), this };
        entity.AddComponent<TransformComponent>();

        if (!name.empty())
			entity.AddComponent<TagComponent>(name);

        auto& node = entity.AddComponent<NodeComponent>();
        if (parent) {
            // Add new node to the scene tree
            node.parent = std::make_shared<Entity>(parent);
            NodeComponent& parentNode = parent.GetComponent<NodeComponent>();
            parentNode.children++;
            node.next = parentNode.first;
            parentNode.first = std::make_shared<Entity>(entity);
        }
        
        auto& idComponent = entity.AddComponent<UUIDComponent>();

        m_EntityMap[idComponent.id] = entity;

        return entity;
    }

    void Scene::SubmitToDestroyEntity(Entity entity) {
        bool isValid = m_Registry.valid((entt::entity)entity);
        if (!isValid)
		{
            return;
        }

        SubmitPostUpdateFunc([entity]() {
            entity.m_Scene->DestroyEntity(entity);
        });
    }

    void Scene::DestroyEntity(Entity entity, bool excludeChildren, bool first) {
        if (!entity)
			return;

        NodeComponent& node = entity.GetComponent<NodeComponent>();

        if (!excludeChildren) {
            // Recursively delete all children
            for (size_t i = 0; i < node.children; i++) {
                std::shared_ptr<Entity> childToDestroy = node.first.lock();
                node.first = childToDestroy->GetComponent<NodeComponent>().next;
                DestroyEntity(*childToDestroy, excludeChildren, false);
            }
        } else {
            // Reparent all children
            std::shared_ptr<Entity> currentChild = node.first.lock();
            for (size_t i = 0; i < node.children; i++) {
                NodeComponent currentChildNode = currentChild->GetComponent<NodeComponent>();

                currentChildNode.parent = node.parent;
                node.parent.lock()->GetComponent<NodeComponent>().children++;
                currentChild = currentChildNode.next.lock();
            }
        }

        // TODO: Call OnDestroy on the behaviours here

        UUID64 id = entity.GetUUID();

        m_Registry.destroy(entity);
        m_EntityMap.erase(id);
        node.parent.lock()->GetComponent<NodeComponent>().children--;
    }
}