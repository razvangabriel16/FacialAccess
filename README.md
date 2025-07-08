# Sistem de acces controlat bazat pe encodari faciale (lucrat pe Raspberry PI Zero 2W):

Currently TODO: store the encoding facial vector in a file in TrustZone.

### Rulare: ```make clean && make && make shared && ./detection_system (2> /dev/null) ```
Conectare la SSH intrucat nu am instalat utilitarul ```image magic``` pentru ```display```
-   ## Hardware Overlook
Sistem complex de alegere a celei mai performante camere disponibile conectate la placuta folosind API-ul v4l2 (Video4Linux2) si apeluri IOCTL catre kernel pentru interogarea capabilitatilor fiecarei camere gasite.
- ## Smart Image Capturing 
 Am capturat o poza in format yuyv cu ajutorul apelului ioctl (cu gandul ca as putea sa optimizez cu NEON datorita structurii registrelor). Dupa ce am cerut kernelului sa aloce un buffer pentru captura video(VIDIOC_REQBUFS) si am obtinut detalii(VIDIOC_QUERYBUF), am mapat bufferul in spatiul de memorie al procesului curent. Astfel mmap imi intoarce un pointer (uint8_t) care indica catre acest buffer partajat astfel incat sa nu trebuiasca sa copiem din kernel-space in user-space, reducand latenta.
- ## CLAHE (Equalization) 
Am realizat o implementare manuala a algoritmului CLAHE, folosit pentru imbunatatirea contrastului in imagini, pastrand in acelasi timp detaliile locale si evitand artefactele produse de egalizarea globala a histogramei. Procesul este descris astfel:
Procesul include: Procesul include urmatorii pasi esentiali:
-   **Impartirea imaginii in tiles**: imaginea este segmentata in blocuri mici, pentru a permite prelucrarea locala a contrastului.
-   **Calculul histogramei pe fiecare tile**: se analizeaza distributia valorilor de intensitate pentru fiecare zona a imaginii.
-   **Aplicarea unui prag de contrast (clipping)**: pentru a limita intensificarea excesiva a contrastului, valorile din histograma sunt taiate peste un anumit prag, iar excesul este redistribuit normal.
-   **Calcularea CDF-ului (cumulative distribution function)**: se genereaza functia de mapare care transforma valorile pixelilor pe baza histogramei ajustate.
-   **Interpolare biliniara intre tiles**: pentru a asigura o tranzitie lina intre blocuri si a evita artefactele vizuale, se foloseste interpolare biliniara intre valorile calculate pentru fiecare tile.
-  ## ARMv8 ASM insertions
 In limbaj de asamblare pentru ARM v8 Cortex A53 am scris cod pentru o conversie din yuyv -> pgm (extractia luminiscentei Y) si o constructie a unei matrice integrale(construita cu programare dinamica a.i. sa avem acces in O(1)) Aceasta o voi folosi pentru BlurEffect (TODO).
- ## Level Set Contouring (Geodesic Approach) + Cubic Spline Interpolation.
  <img width="512" alt="image" src="https://github.com/user-attachments/assets/3e5a5d4c-b788-4ec8-8143-04a68f131d03" />
  <img width="598" alt="image" src="https://github.com/user-attachments/assets/0676f5c9-620d-4575-a649-c5848c9e6b77" />
Referinta de la care am studiat algoritmul (partea matematica foarte stufoasa ):  [https://research-repository.st-andrews.ac.uk/bitstream/handle/10023/15903/IET_IPR.2018.5796.pdf;jsessionid=1EAF0A5538B64874945F79DDD86C9669?sequence=1](https://research-repository.st-andrews.ac.uk/bitstream/handle/10023/15903/IET_IPR.2018.5796.pdf;jsessionid=1EAF0A5538B64874945F79DDD86C9669?sequence=1) 
In aceasta parte, am folosit RungeKutta2 pentru rezolvarea PDE, nu Euler. Initial am vrut sa implementez RK4 dar dura foarte mult la rulare, erau enrom de multe computatii. Si acum sunt comparativ cu solutia intitiala, dar am obtinut exactitate sporita. Pentru ca nu putem lasa un numar prea mare de iteratii, obtinem o masca dupa LSC si apoi interpolam punctele din masca cu cubic spline (C2) sau Hermite, pentru a obtine acuratete mai mare. In script-ul pentru interpolare am apelat pentru prima oara o functie asm din python cu shared library ctypes.
<img width="179" alt="image" src="https://github.com/user-attachments/assets/11e120c7-ed69-4cdb-88b2-2f37ad8099d2" />
<img width="179" alt="image" src="https://github.com/user-attachments/assets/349d175c-c1bf-419e-9fc8-fe021fb6a827" />
<img width="179" alt="image" src="https://github.com/user-attachments/assets/abb083c7-9466-4e1e-b684-7c5dbc33d247" />

  
### TODO:
 Pentru verificare constanta a temperaturii, statusul throttling-ului (limitări din cauza supraîncălzirii sau alimentării slabe) cu ajutorul utilitarului oficial de diagnostic: ```vcgencmd```
sau cu ```/sys/class/thermal/thermal_zone0/temp```. Pentru frecventa curenta a CPU-ului pot folosi ```top```. Pot rula asta intr-un thread separat, iar la final pot obtine statistici de dupa fiecare excutie.
Pe viitor:
-   Daca voi avea timp, voi face acest contouring si cu implementare de min-heap + Djkastra (strict pentru comparatie timp pt computatie cu implementarea curenta)
-   Optimizari in asm cu registrele NEON (inca citesc)
-   Obtinere vectori de encodare faciala cu mobilefacenet.tflite
-   Stocarea lor intr-o baza de date remote
-   Fie verificare cu cosinesimilarity fie o alta solutie mai complexa.
