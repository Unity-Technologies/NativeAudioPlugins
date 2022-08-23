using UnityEngine;
using System.Collections;

public class CircularMotion : MonoBehaviour
{
    public Vector3 axis1 = new Vector3(1.0f, 0.0f, 0.0f);
    public Vector3 axis2 = new Vector3(0.0f, 0.0f, 1.0f);
    public float radius1 = 5.0f;
    public float radius2 = 1.0f;
    public float speed1 = 1.0f;
    public float speed2 = 0.31f;

    private Vector3 center;

    // Use this for initialization
    void Start()
    {
        center = gameObject.transform.position;
    }

    // Update is called once per frame
    void Update()
    {
        float t1 = Time.time * speed1;
        float t2 = Time.time * speed2;
        float u = Mathf.Cos(t1) * radius1 + Mathf.Cos(t2) * radius2;
        float v = Mathf.Sin(t1) * radius1 + Mathf.Sin(t2) * radius2;
        gameObject.transform.position = center + axis1 * u + axis2 * v;
    }
}
