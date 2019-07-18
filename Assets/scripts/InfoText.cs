using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class InfoText : MonoBehaviour
{
    public int x = 10;
    public int y = 10;

    public int w = 300;
    public int h = 200;

    public Color color = Color.black;
    public string text;

    // Start is called before the first frame update
    void Start()
    {
        
    }

    // Update is called once per frame
    void OnGUI()
    {
        GUI.color = color;
        GUI.Label(new Rect(x, y, w, h), text);
    }
}
