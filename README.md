Sistem de acces controlat bazat pe encodari faciale (lucrat pe Raspberry PI Zero 2W):
- Sistem complex de alegere a celei mai performante camere disponibile conectate la placuta folosind API-ul v4l2 (Video4Linux2) si apeluri IOCTL catre kernel pentru interogarea capabilitatilor fiecarei camere gasite.
- Am capturat o poza in format yuyv cu ajutorul apelului ioctl (cu gandul ca as putea sa optimizez cu NEON datorita organizarii).
  Dupa ce am cerut kernelului sa aloce un buffer pentru captura video(VIDIOC_REQBUFS) si am obtinut detalii(VIDIOC_QUERYBUF), am mapat bufferul in spatiul de memorie al procesului curent. Astfel mmap imi intoarce un pointer
  (uint8_t) care indica catre acest buffer partajat astfel incat sa nu trebuiasca sa copiem din kernel-space in user-space, reducand latenta.
- Am realizat o implementare manuala a egalizarii de tip CLAHE(histograme, coordonate, interpolare biliniara etc.)
- In limbaj de asamblare pentru ARM v8 Cortex A53 am scris cod pentru o conversie din yuyv -> pgm (extractia luminiscentei Y) si o constructie a unei matrice integrale(construita cu programare dinamica a.i. sa avem acces in O(1))
  Aceasta o voi folosi pentru BlurEfect.
- Am scris in C o implementare manuala pentru Level Set Contouring (Geodesic Approach) - referinta https://research-repository.st-andrews.ac.uk/bitstream/handle/10023/15903/IET_IPR.2018.5796.pdf;jsessionid=1EAF0A5538B64874945F79DDD86C9669?sequence=1
  As vrea ca in loc sa folosesc metoda clasica de rezolvare a PDE sa folosesc RungeKutta4. 
Pe viitor:
- Daca voi avea timp, voi face acest contouring si cu implementare de min-heap + Djkastra (strict pentru comparatie timp pt computatie)
- optimizari in asm cu registrele NEON (inca citesc)
- obtinere vectori de encodare faciala cu mobilefacenet.tflite
- stocarea lor intr-o baza de date remote
- fie verificare cu cosinesimilarity fie altceva.
