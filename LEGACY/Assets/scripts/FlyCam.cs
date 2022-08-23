using UnityEngine;
using System.Collections;

public class FlyCam : MonoBehaviour
{
    public float lookSpeed = 5.0f;
    public float moveSpeed = 1.0f;

    public float rotationX = 0.0f;
    public float rotationY = 0.0f;

    void Update()
    {
        if (Input.GetMouseButton(0))
        {
            rotationX += Input.GetAxis("Mouse X") * lookSpeed;
            rotationY += Input.GetAxis("Mouse Y") * lookSpeed;
            rotationY = Mathf.Clamp(rotationY, -90.0f, 90.0f);
        }

        transform.localRotation  = Quaternion.AngleAxis(rotationX, Vector3.up);
        transform.localRotation *= Quaternion.AngleAxis(rotationY, Vector3.left);

        transform.position += transform.forward * moveSpeed * Input.GetAxis("Vertical");
        transform.position += transform.right * moveSpeed * Input.GetAxis("Horizontal");
        transform.position += transform.up * 3 * moveSpeed * Input.GetAxis("Mouse ScrollWheel");
    }
}
