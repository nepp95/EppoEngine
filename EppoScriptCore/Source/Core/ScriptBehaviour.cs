using EppoScriptCore.Scene;

namespace EppoScriptCore.Core
{
    public abstract class ScriptBehaviour
    {
        public ulong Id { get; internal set; }
        public Entity Entity => new Entity(Id);

        public virtual void OnCreate()
        {

        }

        public virtual void OnUpdate(float timestep)
        {

        }

        public virtual void OnDestroy()
        { 
        
        }
    }
}
