using UnityEngine;
using System.Collections;

public class PhysicsSpawner : MonoBehaviour
{
    public GameObject[] prototypes;
    public int maxobjects = 500;

    private GameObject[] spawned = null;
    private int counter = 0;
    private System.Random random = new System.Random();

    void Start()
    {
        spawned = new GameObject[maxobjects];
    }

    void Update()
    {
        if (spawned == null)
            return;

        if (counter < spawned.Length)
        {
            float spawnRadius = GetComponent<SphereCollider>().radius;
            var index = random.Next(0, prototypes.Length);
            var newGO = Instantiate(prototypes[index]);
            newGO.transform.position = transform.position + new Vector3((float)(random.NextDouble() * 2.0f - 1.0f) * spawnRadius, (float)(random.NextDouble() * 2.0f - 1.0f) * spawnRadius, (float)(random.NextDouble() * 2.0f - 1.0f) * spawnRadius);
            spawned[counter] = newGO;
            //Debug.Log("Spawned " + prototypes[index]);
        }
        else if (counter >= spawned.Length + 100)
        {
            for (int n = 0; n < spawned.Length; n++)
                Destroy(spawned[n]);
            counter = 0;
        }

        counter++;
    }
}
