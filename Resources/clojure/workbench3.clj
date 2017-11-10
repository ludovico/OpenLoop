(ns studio.core
  (:require [clojure.spec.alpha :as s]
            [clojure.string :as str]
            [clojure.data.json :as json]
            [clojure.core.async :as async]
            [clojure.repl :as repl]))

(let [msgid (atom 0)]
  (defn connect [jsonmap]    
    (def socket (java.net.Socket. "127.0.0.1" 8989))
    (def inputstream (.getInputStream socket))
    (def outputstream (.getOutputStream socket))
    (def intbuffer (java.nio.ByteBuffer/allocate 4))
    (.order intbuffer (java.nio.ByteOrder/LITTLE_ENDIAN))
    
    (def message-as-bytes (.getBytes (json/write-str (assoc jsonmap :msgid (swap! msgid inc)))))
    (.putInt intbuffer 0 (alength message-as-bytes))
    
    (.write outputstream (.array intbuffer))
    (.write outputstream message-as-bytes)
    
    (.read inputstream (.array intbuffer))
    (def response-as-bytes (java.nio.ByteBuffer/allocate (.getInt intbuffer 0)))
    (.read inputstream (.array response-as-bytes))
    (json/read-str (String. (.array response-as-bytes))))

  (defn get-sample-time []
    (get (connect {:msg "get-sample-time" :msgid 0}) "sample"))

  (defn synchronize-time []
    (let [srate (get (connect {:msg "samplerate" :msgid 1}) "samplerate")
          before-nano (System/nanoTime)
          sample (connect {:msg "get-sample-time" :msgid 2})
          after-nano (System/nanoTime)]
      (def samplerate srate)
      (let [before (/ before-nano 1000000000)
            after (/ after-nano 1000000000)
            estimated-sample-t (/ (+ after before) 2)]
        (def hosttime {:systime-before-call (double before)
                       :systime-after-call (double after)
                       :systime-difference (double (- after before))
                       :estimated-t (double estimated-sample-t)
                       :sample (get sample "sample")})))))

(defn test-synchronize-time []
  (let [estimated-sample (int (+ (:sample hosttime)
                                 (* samplerate (- (/ (System/nanoTime) 1000000000) (:estimated-t hosttime)))))
        sample (get-sample-time)]
    {:estimated-sample-time estimated-sample :sample-time sample :off-by (- estimated-sample sample)}))

(defn now [] (:estimated-sample-time (test-synchronize-time)))

(defn createAce []
  (connect {:msg "load-plugin" :path "c:/code/c++/juce/openloop/resources/desktop/plugins/ACE.xml" :outputs [2]})
  #_(connect {:msg "load-plugin" :path "c:/code/c++/juce/openloop/resources/desktop/plugins/FM8.xml"})
  (connect {:msg "load-plugin" :path "c:/code/c++/juce/openloop/resources/desktop/plugins/Protoverb.xml" :outputs [2]})
  (connect {:msg "load-buffer-processor" :type "copy" :from 10 :to 12})
  (connect {:msg "load-buffer-processor" :type "copy" :from 11 :to 13})
  (connect {:msg "load-buffer-processor" :type "copy" :from 12 :to 2})
  (connect {:msg "load-buffer-processor" :type "copy" :from 13 :to 3})
  (connect {:msg "plugin-open-editor" :id 0})
  (connect {:msg "plugin-open-editor" :id 1})
  (connect {:msg "calculate" :id 0 :order 0.0 :buffers-to-use [10 11] :start-sample (+ (now) 44100)})
  (connect {:msg "calculate" :id 2 :order 0.5 :buffers-to-use [] :start-sample (+ (now) 44100)})
  (connect {:msg "calculate" :id 3 :order 0.6 :buffers-to-use [] :start-sample (+ (now) 44100)})
  (connect {:msg "calculate" :id 1 :order 1.0 :buffers-to-use [12 13] :start-sample (+ (now) 44100)})
  (connect {:msg "calculate" :id 4 :order 1.5 :buffers-to-use [] :start-sample (+ (now) 44100)})
  (connect {:msg "calculate" :id 5 :order 1.6 :buffers-to-use [] :start-sample (+ (now) 44100)})
  (connect {:msg "add-midi-connection" :source-id "m03" :dest-id 0 :start-sample (+ (now) 44100)}))

(defn loadFile []
  #_(connect {:msg "load-audio-file" :path "C:/samples/musicradar/90s-ambient-samples/90s Ambient_3/SoundScapes/Scape01(C).wav"})
  (connect {:msg "load-audio-file" :path "E:/Samples/Battery 4 Factory Library/Samples/One Shots/Ambience/Ambience Einoma 2.wav"})
  #_(connect {:msg "load-clip" :file-id 0 :from-sample 0 :length 150000})
  )

(defn playClip []
  (connect {:msg "load-audio-file" :path "E:/Samples/Battery 4 Factory Library/Samples/One Shots/Ambience/Ambience Einoma 2.wav"})
  (connect {:msg "load-clip" :file-id 0 :from-sample 0 :length 44100})
  (connect {:msg "calculate" :id 2 :start-sample (- (now) 22000) :order 1.0 :buffers-to-use [2 3]}))

(defn beat->sample [bpm sampleoffset]
  (let [factor (* samplerate (/ 60 bpm))]
    (fn [map-with-t]
      (assoc map-with-t :sample (int (+ sampleoffset (* factor (:t map-with-t))))))))

(defn midi-note
  ([t len note] (midi-note t len note 64 1 0))
  ([t len note vel] (midi-note t len note vel 1 0))
  ([t len note vel ch] (midi-note t len note vel ch 0))
  ([t len note vel ch vel-off]
   (list {:type "on" :t t :ch ch :note note :vel vel} {:type "off" :t (+ t len) :ch ch :note note :vel vel-off})))

(defn queue-midi-seq [id midi-seq bpm scheduled-sample]
  (let [fut (future
              (let [seq (map (beat->sample bpm scheduled-sample) (flatten midi-seq))
                    [batch tail] (split-with #(< (:sample %)
                                                 (+ samplerate ;one second batch
                                                    scheduled-sample)) seq)]
                (connect {:msg "plugin-queue-midi"
                          :id id
                          :midi (doall batch)})

                (let [mem (atom [batch tail])]
                  (doseq [x (range)]
                    (let [[b t] (split-with #(< (:sample %)
                                                (+ (* (+ x 1) 10 samplerate)
                                                   scheduled-sample)) (second @mem))]
                      (connect {:msg "plugin-queue-midi"
                                :id id
                                :midi (doall b)})
                      (swap! mem (fn [x] [b t]))
                      (if (= x 0) (Thread/sleep 1000) (Thread/sleep 10000)))))))]
    (fn []
      (future-cancel fut)
      (connect {:msg "plugin-clear-queue-midi" :msgid 123 :id id}))))

(def sample-generator-1 (map (fn [t len note] (midi-note (* t 0.5) len note)) (iterate inc 0) (cycle [0.1 0.1 0.2]) (cycle [50 57 65 62])))


